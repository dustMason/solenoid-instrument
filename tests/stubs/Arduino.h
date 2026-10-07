#pragma once
#include <stdint.h>
typedef uint8_t byte;
static const int LOW = 0, HIGH = 1, OUTPUT = 1;
extern uint32_t fakeNow, fakeInterruptMask;
extern bool fakePins[32];
inline uint32_t micros() { return fakeNow; }
inline void __disable_irq() { fakeInterruptMask = 1; }
inline void __enable_irq() { fakeInterruptMask = 0; }
inline void digitalWrite(int pin, int value) { fakePins[pin] = value; }
inline void digitalWriteFast(int pin, int value) { fakePins[pin] = value; }
inline void pinMode(int, int) {}
struct FakeUSBMIDI {
  void (*noteOn)(byte, byte, byte) = nullptr;
  void (*noteOff)(byte, byte, byte) = nullptr;
  void (*controlChange)(byte, byte, byte) = nullptr;
  void (*clock)() = nullptr;
  void (*start)() = nullptr;
  void (*resume)() = nullptr;
  void (*stop)() = nullptr;
  void (*reset)() = nullptr;
  void setHandleNoteOn(void (*fn)(byte, byte, byte)) { noteOn = fn; }
  void setHandleNoteOff(void (*fn)(byte, byte, byte)) { noteOff = fn; }
  void setHandleControlChange(void (*fn)(byte, byte, byte)) { controlChange = fn; }
  void setHandleClock(void (*fn)()) { clock = fn; }
  void setHandleStart(void (*fn)()) { start = fn; }
  void setHandleContinue(void (*fn)()) { resume = fn; }
  void setHandleStop(void (*fn)()) { stop = fn; }
  void setHandleSystemReset(void (*fn)()) { reset = fn; }
  bool read() { return false; }
};
extern FakeUSBMIDI usbMIDI;
