#include "solenoid_engine.h"

namespace solenoid {
namespace {
uint32_t smaller(uint32_t a, uint32_t b) { return a < b ? a : b; }
uint32_t larger(uint32_t a, uint32_t b) { return a > b ? a : b; }
bool due(uint32_t now, uint32_t at) { return int32_t(now - at) >= 0; }
uint32_t mapValue(uint8_t value, uint32_t low, uint32_t high) {
  return low + uint32_t(value) * (high - low) / 127;
}
uint32_t envelopeTime(uint8_t value) {
  // Quadratic taper gives short attacks while retaining a 2s range.
  return uint32_t(value) * value * 2000000ULL / (127 * 127);
}
uint32_t tapWidth(const Voice& v, uint8_t velocity) {
  return larger(kTickUs, mapValue(v.settings.parameters[4], 1000, kMaxPulseUs) * velocity / 127);
}
}  // namespace

Engine::Engine() {
  for (uint8_t i = 0; i < kVoiceCount; ++i) voices_[i].offSince = uint32_t(0) - kMinOffUs;
}

void Engine::silence(uint8_t i) {
  Voice& v = voices_[i];
  v.action = Action::Idle;
  v.stage = Stage::Off;
  v.envelope = 0;
  v.pulsePending = v.pulseActive = v.flamPending = false;
  v.ownerChannel = 0;
  // Retriggering and panic never replenish the physical energy budget.
}

void Engine::allOff() {
  for (uint8_t i = 0; i < kVoiceCount; ++i) silence(i);
  clockRunning_ = false;
}

void Engine::start(uint32_t now) { allOff(); clockRunning_ = true; lastClock_ = now; }
void Engine::clock(uint32_t now) { if (clockRunning_) lastClock_ = now; }

void Engine::trigger(uint8_t i, Action action, bool flam, uint8_t channel,
                     uint8_t note, uint8_t velocity, uint32_t period, uint32_t now) {
  silence(i);  // One physical actuator: latest Note On wins across routes.
  Voice& v = voices_[i];
  v.action = action;
  v.ownerChannel = channel; v.ownerNote = note; v.velocity = velocity;
  v.started = v.stageStarted = now;
  v.period = period;
  if (action == Action::Tap) {
    v.pulseWidth = tapWidth(v, velocity);
    v.pulsePending = true;
    v.flamPending = flam;
    v.flamAt = now + mapValue(v.settings.parameters[5], 12000, 120000);
    v.flamWidth = v.pulseWidth * v.settings.parameters[6] / 127;
    if (v.flamWidth < kTickUs) v.flamPending = false;
  } else {
    v.stage = Stage::Attack;
    v.envelopeUpdated = now - 1000;
    v.nextCycle = now;
    updateEnvelope(v, now);
  }
}

void Engine::noteOn(uint8_t channel, uint8_t note, uint8_t velocity, uint32_t now) {
  if (note > 127 || velocity > 127 || channel < 1 || channel > 16) return;
  if (velocity == 0) { noteOff(channel, note, now); return; }
  uint8_t i, articulation = 3;
  if (channel >= 1 && channel <= 4) {
    i = channel - 1;
  } else if (channel == kDrumChannel && note >= 36 && note <= 51) {
    i = (note - 36) % 4;
    articulation = (note - 36) / 4;
  } else {
    return;
  }
  const Voice& v = voices_[i];
  const bool buzz = articulation == 2 || (articulation == 3 && v.settings.mode == Mode::Buzz);
  const bool flam = articulation == 1 || (articulation == 3 && v.settings.flam);
  const uint8_t pitch = channel == kDrumChannel ? v.settings.parameters[8] : note;
  trigger(i, buzz ? Action::Buzz : Action::Tap, flam, channel, note, velocity,
          periodForNote(pitch), now);
}

void Engine::noteOff(uint8_t channel, uint8_t note, uint32_t now) {
  for (uint8_t i = 0; i < kVoiceCount; ++i) {
    Voice& v = voices_[i];
    if (v.action != Action::Buzz || v.ownerChannel != channel || v.ownerNote != note || v.stage == Stage::Release) continue;
    updateEnvelope(v, now);
    v.releaseLevel = v.envelope;
    v.stage = Stage::Release;
    v.stageStarted = now;
    v.envelopeUpdated = now - 1000;
  }
}

void Engine::setParameter(uint8_t i, uint8_t index, uint8_t value, uint32_t now) {
  Voice& v = voices_[i];
  v.settings.parameters[index] = value;
  v.envelopeUpdated = now - 1000;
  if (index == 8 && v.action == Action::Buzz && v.ownerChannel == kDrumChannel) {
    v.period = periodForNote(value);
    v.nextCycle = now;
  }
}

void Engine::controlChange(uint8_t channel, uint8_t cc, uint8_t value, uint32_t now) {
  if (channel < 1 || channel > 16 || cc > 127 || value > 127) return;
  if (cc == 120 || cc == 123) {
    if (channel == kControlChannel || channel == kDrumChannel) allOff();
    else if (channel <= 4) silence(channel - 1);
    return;
  }
  if (channel != kControlChannel && channel > 4) return;
  if (channel == kControlChannel) {
    if (cc == 16) { if (value < 4) selected_ = value; return; }
    if (cc >= 110 && cc <= 113) { if (value >= 64) selected_ = cc - 110; return; }
  }
  const uint8_t i = channel == kControlChannel ? selected_ : channel - 1;
  Voice& v = voices_[i];
  if (cc == 17 || ((cc == 114 || cc == 115) && value >= 64)) {
    const Mode mode = cc == 114 ? Mode::Tap : cc == 115 ? Mode::Buzz : value < 64 ? Mode::Tap : Mode::Buzz;
    if (mode != v.settings.mode) { silence(i); v.settings.mode = mode; }
  } else if (cc == 18) {
    v.settings.flam = value >= 64;
  } else if (cc == 119 && value >= 64) {
    silence(i); v.settings = Settings();
  } else if (cc >= 20 && cc <= 28) {
    setParameter(i, cc - 20, value, now);
  } else if (cc >= 70 && cc <= 78) {
    // Arturia Relative 1: 61..63 decrement; 65..67 increment. Ignore neutral
    // 0/64 values instead of interpreting 0 as a large negative jump.
    if (value < 61 || value > 67 || value == 64) return;
    const uint8_t index = cc - 70;
    int updated = int(v.settings.parameters[index]) + int(value) - 64;
    if (updated < 0) updated = 0;
    if (updated > 127) updated = 127;
    setParameter(i, index, uint8_t(updated), now);
  }
}

void Engine::updateEnvelope(Voice& v, uint32_t now) {
  if (v.stage == Stage::Attack) {
    const uint32_t length = envelopeTime(v.settings.parameters[0]);
    const uint32_t elapsed = now - v.stageStarted;
    if (elapsed < length) { v.envelope = uint64_t(elapsed) * 1024 / length; return; }
    v.stage = Stage::Decay; v.stageStarted += length; v.envelope = 1024;
  }
  if (v.stage == Stage::Decay) {
    const uint32_t length = envelopeTime(v.settings.parameters[1]);
    const uint32_t elapsed = now - v.stageStarted;
    const uint32_t sustain = uint32_t(v.settings.parameters[2]) * 1024 / 127;
    if (elapsed < length) { v.envelope = 1024 - uint64_t(elapsed) * (1024 - sustain) / length; return; }
    v.stage = Stage::Sustain;
  }
  if (v.stage == Stage::Sustain) v.envelope = uint32_t(v.settings.parameters[2]) * 1024 / 127;
  if (v.stage == Stage::Release) {
    const uint32_t length = envelopeTime(v.settings.parameters[3]);
    const uint32_t elapsed = now - v.stageStarted;
    if (elapsed < length) { v.envelope = v.releaseLevel - uint64_t(elapsed) * v.releaseLevel / length; return; }
    v.envelope = 0; v.stage = Stage::Off; v.action = Action::Idle;
  }
}

uint8_t Engine::tick(uint32_t now) {
  const uint32_t dt = ticking_ ? uint32_t(now - lastTick_) : 0;
  // Stop on a late timer rather than replaying a burst of missed edges.
  if (dt > kTickUs * 4) allOff();
  ticking_ = true; lastTick_ = now;
  if (clockRunning_ && uint32_t(now - lastClock_) > kClockTimeoutUs) allOff();
  uint8_t result = 0;
  for (uint8_t i = 0; i < kVoiceCount; ++i) {
    Voice& v = voices_[i];
    // Charge for the previous interval's actual output state, even if a MIDI
    // callback has just retriggered the voice. Never reset credits on Note On.
    const uint32_t elapsed = smaller(dt, 100000);
    if (v.output) {
      const uint32_t cost = elapsed * (1000 - kMaxDutyPermille);
      v.credits = v.credits > cost ? v.credits - cost : 0;
    } else {
      v.credits = smaller(kMaxPulseUs * 1000, v.credits + elapsed * kMaxDutyPermille);
    }
    if (v.output && uint32_t(now - v.onSince) >= kMaxPulseUs) v.pulseActive = false;
    if (v.pulseActive && due(now, v.pulseEnd)) v.pulseActive = false;

    if (v.action == Action::Tap) {
      if (v.flamPending && due(now, v.flamAt)) {
        v.flamPending = false; v.pulsePending = true; v.pulseWidth = v.flamWidth;
      }
      if (v.pulsePending && !v.output && uint32_t(now - v.offSince) >= kMinOffUs) {
        v.pulsePending = false;
        // Drop a budget-limited hit instead of queuing a surprising late tap.
        if (v.credits >= v.pulseWidth * (1000 - kMaxDutyPermille)) {
          v.pulseEnd = now + v.pulseWidth; v.pulseActive = true;
        }
      }
      if (!v.pulseActive && !v.pulsePending && !v.flamPending) v.action = Action::Idle;
    } else if (v.action == Action::Buzz) {
      if (uint32_t(now - v.started) >= kMaxBuzzUs) {
        silence(i);
      } else {
        if (uint32_t(now - v.envelopeUpdated) >= 1000) {
          updateEnvelope(v, now); v.envelopeUpdated = now;
          const uint32_t duty = mapValue(v.settings.parameters[7], 10, kMaxDutyPermille);
          v.pulseWidth = uint64_t(v.period) * duty * v.envelope * v.velocity / (1000ULL * 1024 * 127);
          v.pulseWidth = smaller(kMaxPulseUs, v.pulseWidth);
        }
        if (v.action == Action::Buzz && due(now, v.nextCycle)) {
          v.nextCycle += v.period;
          if (due(now, v.nextCycle)) v.nextCycle = now + v.period;
          if (!v.output && uint32_t(now - v.offSince) >= kMinOffUs && v.pulseWidth >= kTickUs &&
              v.credits >= v.pulseWidth * (1000 - kMaxDutyPermille)) {
            v.pulseEnd = now + v.pulseWidth; v.pulseActive = true;
          }
        }
      }
    }
    // A budget cut ends the pulse; recovering credits must never split one hit
    // into extra rising edges. Every new physical edge observes the off-time.
    if (v.credits < kTickUs * (1000 - kMaxDutyPermille)) v.pulseActive = false;
    const bool output = v.action != Action::Idle && v.pulseActive &&
                        (v.output || uint32_t(now - v.offSince) >= kMinOffUs);
    if (output && !v.output) v.onSince = now;
    if (!output && v.output) v.offSince = now;
    v.output = output;
    if (output) result |= uint8_t(1 << i);
  }
  return result;
}
}  // namespace solenoid
