#ifndef SOLENOID_ENGINE_H
#define SOLENOID_ENGINE_H
#include <stdint.h>

namespace solenoid {
static const uint8_t kVoiceCount = 4;
static const uint8_t kDrumChannel = 10;
static const uint8_t kControlChannel = 16;
static const uint32_t kTickUs = 50;
// Initial software limits, NOT a substitute for the coil/driver ratings.
static const uint32_t kMaxPulseUs = 10000;
static const uint32_t kMinOffUs = 1000;
static const uint32_t kMaxDutyPermille = 250;
static const uint32_t kMaxBuzzUs = 5000000;
static const uint32_t kClockTimeoutUs = 500000;

enum class Mode : uint8_t { Tap, Buzz };
enum class Action : uint8_t { Idle, Tap, Buzz };
enum class Stage : uint8_t { Off, Attack, Decay, Sustain, Release };

struct Settings {
  Mode mode = Mode::Buzz;
  bool flam = false;
  // CC20..28: A D S R, tap width, flam gap/strength, buzz duty, drum pitch.
  uint8_t parameters[9] = {3, 16, 90, 12, 42, 27, 76, 74, 45};
};

struct Voice {
  Settings settings;
  Action action = Action::Idle;
  Stage stage = Stage::Off;
  uint8_t ownerChannel = 0, ownerNote = 0, velocity = 0;
  uint32_t started = 0, stageStarted = 0, nextCycle = 0, period = 0;
  uint32_t pulseEnd = 0, flamAt = 0, flamWidth = 0;
  uint32_t pulseWidth = 0, envelopeUpdated = 0;
  uint16_t envelope = 0, releaseLevel = 0;
  bool pulsePending = false, pulseActive = false, flamPending = false;
  bool output = false;
  uint32_t onSince = 0, offSince = 0;
  uint32_t credits = kMaxPulseUs * 1000;
};

class Engine {
 public:
  Engine();
  void noteOn(uint8_t channel, uint8_t note, uint8_t velocity, uint32_t now);
  void noteOff(uint8_t channel, uint8_t note, uint32_t now);
  void controlChange(uint8_t channel, uint8_t cc, uint8_t value, uint32_t now);
  void clock(uint32_t now);
  void start(uint32_t now);
  void allOff();
  uint8_t tick(uint32_t now);
  const Voice& voice(uint8_t i) const { return voices_[i]; }
  uint8_t selectedVoice() const { return selected_; }
  static uint32_t periodForNote(uint8_t note);
 private:
  Voice voices_[kVoiceCount];
  uint8_t selected_ = 0;
  bool ticking_ = false, clockRunning_ = false;
  uint32_t lastTick_ = 0, lastClock_ = 0;
  void silence(uint8_t i);
  void trigger(uint8_t i, Action action, bool flam, uint8_t channel,
               uint8_t note, uint8_t velocity, uint32_t period, uint32_t now);
  void updateEnvelope(Voice& voice, uint32_t now);
  void setParameter(uint8_t i, uint8_t index, uint8_t value, uint32_t now);
};
}  // namespace solenoid
#endif
