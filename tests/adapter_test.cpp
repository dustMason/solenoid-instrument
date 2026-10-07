#include <stdio.h>
#include <stdlib.h>
#include "Arduino.h"
uint32_t fakeNow = 0, fakeInterruptMask = 0;
bool fakePins[32] = {};
FakeUSBMIDI usbMIDI;
extern "C" { volatile uint8_t usb_configuration = 1; }
#include "../solenoids.ino"
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)

void resetFixture() {
  instrument = solenoid::Engine();
  readIndex = writeIndex = 0; overflowed = false;
  fakeNow = fakeInterruptMask = 0; usb_configuration = 1;
  for (bool& pin : fakePins) pin = true;
  setup();
  for (uint8_t pin : kPins) CHECK(!fakePins[pin]);
  CHECK(outputTimer.callback == updateOutputs && outputTimer.interval == 50 && outputTimer.irqPriority == 64);
}
void run(uint32_t duration, bool heartbeat = true) {
  for (uint32_t t = 0; t < duration; t += solenoid::kTickUs) {
    if (heartbeat) loop();
    outputTimer.callback(); fakeNow += solenoid::kTickUs;
  }
}
void allPinsLow() { for (uint8_t pin : kPins) CHECK(!fakePins[pin]); }

void callbacksAndFIFO() {
  resetFixture();
  // The control and note messages must execute in FIFO order in the timer.
  usbMIDI.controlChange(1, 17, 0); usbMIDI.noteOn(1, 60, 127); usbMIDI.noteOff(1, 60, 0);
  allPinsLow(); run(50);
  CHECK(fakePins[9] && !fakePins[10] && !fakePins[11] && !fakePins[12]);
  run(20000); allPinsLow();
  usbMIDI.noteOn(1, 60, 0); run(50); allPinsLow();
  for (unsigned cycle = 0; cycle < 100; ++cycle) { // Wrap ring indices repeatedly.
    usbMIDI.noteOn(10, 39, 127); run(50); CHECK(fakePins[12]); run(20000); allPinsLow();
  }
  fakeInterruptMask = 1; usbMIDI.clock(); CHECK(fakeInterruptMask == 1);
  fakeInterruptMask = 0; usbMIDI.clock(); CHECK(fakeInterruptMask == 0);
}

void queueOverflow() {
  resetFixture(); usbMIDI.noteOn(10, 36, 127); run(50); CHECK(fakePins[9]);
  for (unsigned i = 0; i < 80; ++i) usbMIDI.noteOn(10, 37, 127);
  CHECK(overflowed); run(50); allPinsLow();
  CHECK(!overflowed && readIndex == writeIndex);
  run(10000); allPinsLow();
  usbMIDI.noteOn(10, 37, 127); run(50); CHECK(fakePins[10]);
}

void disconnectionAndStall() {
  resetFixture(); usbMIDI.noteOn(10, 36, 127); run(50); CHECK(fakePins[9]);
  usb_configuration = 0; run(50); allPinsLow();
  usbMIDI.noteOn(10, 37, 127); run(50); allPinsLow();
  usb_configuration = 1; run(10000); allPinsLow();
  usbMIDI.noteOn(1, 45, 127); run(100000);
  CHECK(instrument.voice(0).action == solenoid::Action::Buzz);
  run(250100, false); allPinsLow(); CHECK(instrument.voice(0).action == solenoid::Action::Idle);
}

void transportAndPanic() {
  resetFixture(); usbMIDI.start(); usbMIDI.noteOn(1, 45, 127); run(20000);
  usbMIDI.clock(); run(50);
  usbMIDI.stop(); run(50); allPinsLow(); CHECK(instrument.voice(0).action == solenoid::Action::Idle);
  run(1000); usbMIDI.noteOn(10, 36, 127); run(50); CHECK(fakePins[9]);
  usbMIDI.reset(); run(50); allPinsLow();
  run(1000); usbMIDI.noteOn(10, 38, 127); run(50); CHECK(fakePins[11]);
  usbMIDI.controlChange(16, 120, 0); run(50); allPinsLow();
  usbMIDI.resume(); usbMIDI.noteOn(1, 45, 127); run(501000);
  allPinsLow(); CHECK(instrument.voice(0).action == solenoid::Action::Idle);
}

int main() {
  callbacksAndFIFO(); queueOverflow(); disconnectionAndStall(); transportAndPanic();
  puts("PASS: 4 USB/timer adapter tests (stubbed hardware)");
}
