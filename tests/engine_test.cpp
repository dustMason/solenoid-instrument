#include "solenoid_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <vector>

using namespace solenoid;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)

struct Sim {
  Engine engine;
  uint32_t now;
  uint8_t previous = 0;
  std::vector<uint32_t> rises[4], falls[4];
  uint64_t highTime[4] = {};
  explicit Sim(uint32_t start = 0) : now(start) { engine.tick(now); }
  uint8_t tick() {
    const uint8_t mask = engine.tick(now);
    for (unsigned i = 0; i < 4; ++i) {
      const uint8_t bit = 1 << i;
      if ((mask & bit) && !(previous & bit)) rises[i].push_back(now);
      if (!(mask & bit) && (previous & bit)) falls[i].push_back(now);
      if (mask & bit) highTime[i] += kTickUs;
    }
    previous = mask; now += kTickUs;
    return mask;
  }
  void run(uint32_t duration) { for (uint32_t elapsed = 0; elapsed < duration; elapsed += kTickUs) tick(); }
  void cc(uint8_t channel, uint8_t number, uint8_t value) { engine.controlChange(channel, number, value, now); }
  void on(uint8_t channel, uint8_t note, uint8_t velocity = 127) { engine.noteOn(channel, note, velocity, now); }
  void off(uint8_t channel, uint8_t note) { engine.noteOff(channel, note, now); }
  void fullBuzz(uint8_t channel) { cc(channel, 20, 0); cc(channel, 21, 0); cc(channel, 22, 127); }
  void checkEdges() {
    for (unsigned v = 0; v < 4; ++v) {
      for (size_t i = 0; i < falls[v].size(); ++i) {
        CHECK(uint32_t(falls[v][i] - rises[v][i]) <= kMaxPulseUs);
        if (i + 1 < rises[v].size()) CHECK(uint32_t(rises[v][i + 1] - falls[v][i]) >= kMinOffUs);
      }
    }
  }
};

void tapLeadingEdge() {
  Sim s;
  s.cc(1, 17, 0); s.on(1, 60); s.run(500);
  s.off(1, 60); s.run(20000); // Gate length cannot truncate a tap.
  CHECK(s.rises[0].size() == 1); CHECK(s.falls[0].size() == 1);
  CHECK(s.falls[0][0] >= 3950 && s.falls[0][0] <= 4000);
  s.on(1, 60, 0); s.off(1, 60); s.run(20000);
  CHECK(s.rises[0].size() == 1);
  s.on(1, 90); s.run(20000); CHECK(s.rises[0].size() == 2);
  s.checkEdges();
}

void tapVelocity() {
  Sim soft, hard;
  soft.on(10, 36, 32); hard.on(10, 36, 127);
  soft.run(20000); hard.run(20000);
  CHECK(soft.rises[0].size() == 1 && hard.rises[0].size() == 1);
  CHECK(hard.highTime[0] >= 3 * soft.highTime[0]);
}

void flamAndCancellation() {
  Sim s;
  s.cc(1, 25, 0); // 12ms between leading edges.
  s.on(10, 40); s.off(10, 40); s.run(30000);
  CHECK(s.rises[0].size() == 2);
  CHECK(s.rises[0][1] - s.rises[0][0] == 12000);
  CHECK(s.falls[0][1] - s.rises[0][1] < s.falls[0][0] - s.rises[0][0]);
  s.checkEdges();
  Sim cancel;
  cancel.on(10, 40); cancel.run(8000); cancel.on(10, 36); cancel.run(120000);
  CHECK(cancel.rises[0].size() == 2); // New articulation cancels the old flam.
  Sim stop;
  stop.on(10, 40); stop.run(500); stop.engine.allOff(); CHECK(stop.tick() == 0);
  stop.run(150000); CHECK(stop.rises[0].size() == 1);
}

void routesAndUnrecognizedInput() {
  Sim s;
  for (uint8_t note = 36; note <= 39; ++note) s.on(10, note);
  CHECK(s.tick() == 15); s.run(20000);
  for (unsigned i = 0; i < 4; ++i) CHECK(s.rises[i].size() == 1);
  s.on(10, 35); s.on(10, 52); s.on(5, 60); s.on(16, 60); s.on(0, 60); s.on(1, 255);
  s.run(20000);
  for (unsigned i = 0; i < 4; ++i) CHECK(s.rises[i].size() == 1);
  for (uint8_t channel = 1; channel <= 4; ++channel) { s.cc(channel, 17, 0); s.on(channel, 100); }
  CHECK(s.tick() == 15);
}

void drumArticulations() {
  Sim s;
  s.cc(1, 17, 0); s.on(10, 44);
  CHECK(s.engine.voice(0).action == Action::Buzz); // Explicit buzz overrides tap mode.
  CHECK(s.engine.voice(0).period == Engine::periodForNote(45));
  s.cc(1, 28, 69); CHECK(s.engine.voice(0).period == 2273);
  s.off(10, 44); CHECK(s.engine.voice(0).stage == Stage::Release);
  s.on(10, 48); CHECK(s.engine.voice(0).action == Action::Tap);
  s.cc(1, 17, 127); s.on(10, 36); CHECK(s.engine.voice(0).action == Action::Tap);
  s.on(10, 48); CHECK(s.engine.voice(0).action == Action::Buzz);
}

void ownershipAndZeroVelocity() {
  Sim s;
  s.on(1, 48); s.run(10000); s.on(1, 50);
  s.off(1, 48); CHECK(s.engine.voice(0).stage != Stage::Release);
  s.off(10, 50); CHECK(s.engine.voice(0).stage != Stage::Release);
  s.run(10000); s.on(1, 50, 0); CHECK(s.engine.voice(0).stage == Stage::Release);
  const uint32_t releaseAt = s.engine.voice(0).stageStarted;
  s.run(1000); s.off(1, 50); CHECK(s.engine.voice(0).stageStarted == releaseAt);
  s.run(30000); CHECK(s.engine.voice(0).action == Action::Idle);
}

void envelope() {
  Sim s;
  s.cc(1, 20, 127); s.cc(1, 21, 127); s.cc(1, 22, 64); s.cc(1, 23, 127);
  s.on(1, 45); s.run(1000000);
  CHECK(s.engine.voice(0).envelope >= 510 && s.engine.voice(0).envelope <= 513);
  s.run(1000000); CHECK(s.engine.voice(0).envelope >= 1022);
  s.run(1000000); CHECK(s.engine.voice(0).stage == Stage::Decay);
  CHECK(s.engine.voice(0).envelope >= 769 && s.engine.voice(0).envelope <= 772);
  s.run(1100000); CHECK(s.engine.voice(0).stage == Stage::Sustain);
  CHECK(s.engine.voice(0).envelope == 516);
  s.off(1, 45); s.run(400000);
  CHECK(s.engine.voice(0).stage == Stage::Release);
  CHECK(s.engine.voice(0).envelope >= 412 && s.engine.voice(0).envelope <= 415);
  // Release can finish normally before the independent five-second ceiling.
  s.cc(1, 23, 0); s.run(2000); CHECK(s.engine.voice(0).action == Action::Idle);
  s.checkEdges();
}

void pitchAndIndependentVoices() {
  CHECK(Engine::periodForNote(69) == 2273); // A4, correcting the old octave offset.
  CHECK(Engine::periodForNote(0) == 50000); CHECK(Engine::periodForNote(127) == 2000);
  Sim s;
  for (uint8_t i = 1; i <= 4; ++i) { s.fullBuzz(i); s.on(i, 44 + i); }
  s.run(150000);
  for (unsigned i = 0; i < 4; ++i) {
    CHECK(s.rises[i].size() >= 8);
    const uint32_t measured = s.rises[i][2] - s.rises[i][1];
    const uint32_t expected = Engine::periodForNote(45 + i);
    CHECK(measured + kTickUs >= expected && measured <= expected + kTickUs);
  }
  s.cc(2, 120, 0); s.run(100);
  CHECK(s.engine.voice(1).action == Action::Idle);
  CHECK(s.engine.voice(0).action == Action::Buzz && s.engine.voice(2).action == Action::Buzz);
  s.checkEdges();
}

void controllerSelectionAndParameters() {
  Sim s;
  s.cc(16, 112, 127); CHECK(s.engine.selectedVoice() == 2);
  s.cc(16, 110, 0); CHECK(s.engine.selectedVoice() == 2);
  s.cc(16, 114, 127); CHECK(s.engine.voice(2).settings.mode == Mode::Tap);
  s.cc(16, 115, 0); CHECK(s.engine.voice(2).settings.mode == Mode::Tap);
  s.cc(16, 115, 127); CHECK(s.engine.voice(2).settings.mode == Mode::Buzz);
  s.cc(16, 18, 127); CHECK(s.engine.voice(2).settings.flam);
  s.cc(16, 20, 12); CHECK(s.engine.voice(2).settings.parameters[0] == 12);
  s.cc(16, 70, 65); CHECK(s.engine.voice(2).settings.parameters[0] == 13);
  s.cc(16, 70, 61); CHECK(s.engine.voice(2).settings.parameters[0] == 10);
  s.cc(16, 70, 0); s.cc(16, 70, 64); s.cc(16, 70, 127);
  CHECK(s.engine.voice(2).settings.parameters[0] == 10);
  s.cc(16, 20, 126); s.cc(16, 70, 67); CHECK(s.engine.voice(2).settings.parameters[0] == 127);
  s.cc(16, 20, 1); s.cc(16, 70, 61); CHECK(s.engine.voice(2).settings.parameters[0] == 0);
  s.cc(1, 20, 80); CHECK(s.engine.voice(0).settings.parameters[0] == 80);
  CHECK(s.engine.voice(2).settings.parameters[0] == 0);
  s.cc(16, 119, 127); CHECK(s.engine.voice(2).settings.parameters[0] == 3);
  s.cc(16, 16, 3); CHECK(s.engine.selectedVoice() == 3);
  s.cc(16, 16, 127); CHECK(s.engine.selectedVoice() == 3);
}

void stopClockAndLifetime() {
  Sim s;
  s.engine.start(s.now); s.on(1, 45); s.run(400000);
  s.engine.clock(s.now); s.run(400000); CHECK(s.engine.voice(0).action == Action::Buzz);
  s.run(110000); CHECK(s.engine.voice(0).action == Action::Idle);
  s.on(1, 45); s.run(600000); CHECK(s.engine.voice(0).action == Action::Buzz); // Free play needs no clock.
  s.run(4400100); CHECK(s.engine.voice(0).action == Action::Idle);
  s.on(10, 40); s.run(500); s.cc(16, 123, 0); CHECK(s.tick() == 0);
  s.run(200000); CHECK(s.engine.voice(0).action == Action::Idle);
  s.on(1, 45); s.engine.start(s.now); CHECK(s.tick() == 0);
}

void missedDeadline() {
  Sim s;
  s.on(10, 36); CHECK(s.tick() == 1);
  s.now += 1000; CHECK(s.tick() == 0); s.run(50000);
  CHECK(s.rises[0].size() == 1);
}

void rollover() {
  Sim s(UINT32_MAX - 8000);
  s.cc(1, 25, 0); s.on(10, 40); s.run(30000);
  CHECK(s.rises[0].size() == 2);
  CHECK(uint32_t(s.rises[0][1] - s.rises[0][0]) == 12000);
  s.checkEdges();
  Sim b(UINT32_MAX - 8000);
  b.fullBuzz(1); b.on(1, 45); b.run(30000); b.off(1, 45); b.run(30000);
  CHECK(b.engine.voice(0).action == Action::Idle); b.checkEdges();
}

void retriggerEnergyBudget() {
  Sim s;
  s.cc(1, 24, 127);
  const uint32_t duration = 2000000;
  for (uint32_t t = 0; t < duration; t += kTickUs) {
    if (t % 11000 == 0) { s.engine.allOff(); s.on(10, 36); }
    s.tick();
  }
  s.engine.allOff(); s.tick(); s.checkEdges();
  CHECK(s.rises[0].size() > 20);
  // The bucket permits a short initial burst, then <=25% long-term duty.
  CHECK(s.highTime[0] <= duration / 4 + kMaxPulseUs);
}

void adversarialEvents() {
  Sim s;
  uint32_t random = 1234567;
  const uint32_t duration = 3000000;
  for (uint32_t t = 0; t < duration; t += kTickUs) {
    random = random * 1664525U + 1013904223U;
    if (t % 250 == 0) {
      const uint8_t channel = 1 + ((random >> 16) & 3);
      switch (random & 7) {
        case 0: s.on(channel, (random >> 8) & 127, (random >> 24) & 127); break;
        case 1: s.off(channel, (random >> 8) & 127); break;
        case 2: s.on(10, 36 + ((random >> 8) & 15)); break;
        case 3: s.cc(channel, 20 + ((random >> 8) % 9), (random >> 24) & 127); break;
        case 4: s.cc(channel, 17, (random >> 24) & 127); break;
        case 5: s.cc(channel, 120, 0); break;
        case 6: s.cc(16, 16, channel - 1); break;
        case 7: s.cc(16, 70 + ((random >> 8) % 9), 61 + ((random >> 24) % 7)); break;
      }
    }
    s.tick();
  }
  s.engine.allOff(); s.tick(); s.checkEdges();
  for (unsigned i = 0; i < 4; ++i) CHECK(s.highTime[i] <= duration / 4 + kMaxPulseUs);
}

int main() {
  typedef void (*Test)();
  const Test tests[] = {tapLeadingEdge, tapVelocity, flamAndCancellation, routesAndUnrecognizedInput,
    drumArticulations, ownershipAndZeroVelocity, envelope, pitchAndIndependentVoices,
    controllerSelectionAndParameters, stopClockAndLifetime, missedDeadline, rollover,
    retriggerEnergyBudget, adversarialEvents};
  for (Test test : tests) test();
  printf("PASS: %zu engine tests\n", sizeof(tests) / sizeof(tests[0]));
}
