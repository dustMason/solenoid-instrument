#include <Arduino.h>
#include <IntervalTimer.h>
#include <util/atomic.h>
#include "solenoid_engine.h"

#if !defined(__MK20DX256__)
#error "Select Teensy 3.1 / 3.2. Other boards have not been validated for this wiring."
#endif
#if !defined(USB_MIDI) && !defined(USB_MIDI_SERIAL)
#error "Select USB Type: MIDI (or Serial + MIDI)."
#endif

// Preserve the original driver connections. Never connect coils to GPIO.
static const uint8_t kPins[4] = {9, 10, 11, 12};
static solenoid::Engine instrument;
static IntervalTimer outputTimer;
extern "C" { extern volatile uint8_t usb_configuration; }

// The timer exclusively owns the engine. USB callbacks only enqueue small,
// bounded messages; no floats, allocation, USB calls or waits run in the ISR.
struct Event { uint8_t type, channel, a, b; };
static const uint8_t kQueueMask = 63;
static volatile Event events[64];
static volatile uint8_t readIndex = 0, writeIndex = 0;
static volatile bool overflowed = false;
static volatile uint32_t loopHeartbeat = 0;

void enqueue(uint8_t type, uint8_t channel, uint8_t a, uint8_t b) {
  const uint32_t interruptsWereDisabled = __get_primask();
  __disable_irq();
  const uint8_t next = (writeIndex + 1) & kQueueMask;
  if (next == readIndex) {
    overflowed = true;
  } else if (!overflowed) {
    events[writeIndex].type = type;
    events[writeIndex].channel = channel;
    events[writeIndex].a = a;
    events[writeIndex].b = b;
    writeIndex = next;
  }
  if (!interruptsWereDisabled) __enable_irq();
}

void onNoteOn(byte channel, byte note, byte velocity) { enqueue(1, channel, note, velocity); }
void onNoteOff(byte channel, byte note, byte velocity) { enqueue(2, channel, note, velocity); }
void onControlChange(byte channel, byte cc, byte value) { enqueue(3, channel, cc, value); }
void onClock() { enqueue(4, 0, 0, 0); }
void onStop() { enqueue(5, 0, 0, 0); }
void onStart() { enqueue(6, 0, 0, 0); }
void onContinue() { enqueue(6, 0, 0, 0); }
void onReset() { enqueue(5, 0, 0, 0); }

void updateOutputs() {
  const uint32_t now = micros();
  if (overflowed || !usb_configuration || uint32_t(now - loopHeartbeat) > 250000) {
    readIndex = writeIndex;
    overflowed = false;
    instrument.allOff();
  } else {
    for (uint8_t n = 0; n < 8 && readIndex != writeIndex; ++n) {
      const uint8_t i = readIndex;
      const Event event = {events[i].type, events[i].channel, events[i].a, events[i].b};
      readIndex = (i + 1) & kQueueMask;
      switch (event.type) {
        case 1: instrument.noteOn(event.channel, event.a, event.b, now); break;
        case 2: instrument.noteOff(event.channel, event.a, now); break;
        case 3: instrument.controlChange(event.channel, event.a, event.b, now); break;
        case 4: instrument.clock(now); break;
        case 5: instrument.allOff(); break;
        case 6: instrument.start(now); break;
      }
    }
  }
  const uint8_t outputs = instrument.tick(now);
  digitalWriteFast(9, (outputs & 1) ? HIGH : LOW);
  digitalWriteFast(10, (outputs & 2) ? HIGH : LOW);
  digitalWriteFast(11, (outputs & 4) ? HIGH : LOW);
  digitalWriteFast(12, (outputs & 8) ? HIGH : LOW);
}

void setup() {
  for (uint8_t i = 0; i < 4; ++i) { digitalWrite(kPins[i], LOW); pinMode(kPins[i], OUTPUT); }
  usbMIDI.setHandleNoteOn(onNoteOn);
  usbMIDI.setHandleNoteOff(onNoteOff);
  usbMIDI.setHandleControlChange(onControlChange);
  usbMIDI.setHandleClock(onClock);
  usbMIDI.setHandleStart(onStart);
  usbMIDI.setHandleContinue(onContinue);
  usbMIDI.setHandleStop(onStop);
  usbMIDI.setHandleSystemReset(onReset);
  loopHeartbeat = micros();
  // Fail closed if the hardware timer cannot be allocated.
  if (!outputTimer.begin(updateOutputs, solenoid::kTickUs)) {
    while (true) { usbMIDI.read(); }
  }
  outputTimer.priority(64);
}

void loop() {
  loopHeartbeat = micros();
  for (uint8_t n = 0; n < 32 && usbMIDI.read(); ++n) { loopHeartbeat = micros(); }
}
