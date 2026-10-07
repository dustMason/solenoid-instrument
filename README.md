# Solenoid instrument: tap / flam / buzz

Four independent solenoid voices for a **Teensy 3.1 or 3.2**, designed for the Arturia BeatStep Pro. The Teensy is a class-compliant **USB MIDI device**. Connect both it and the BeatStep Pro to the Mac; route `BeatStep Pro → Teensy MIDI` in MIDI Bridge. The Mac supplies the USB host. A USB cable directly between the controller and Teensy will not work.

The driver outputs remain **pins 9, 10, 11, 12**, in that order. This preserves the original sketch's wiring. Confirm the actual Teensy model and assembled driver wiring before flashing: the driver-board artwork alone does not identify the Teensy. The build rejects other MCU types and non-MIDI USB modes.

## Build the Mac app

The native MIDI Bridge app's source, Swift package, tests and packaging script are included in [`midi-bridge/`](midi-bridge/). Build on a Mac with Xcode Command Line Tools and Swift 5.9 or later (`xcode-select --install` if needed). No third-party packages or full Xcode project are required.

From this repository's root:

```sh
bash midi-bridge/scripts/build-app.sh
open "midi-bridge/dist/MIDI Bridge.app"
```

The script creates a locally signed release app and `midi-bridge/dist/MIDI Bridge.zip`. It builds for the current Mac's architecture; building on the M1 produces an Apple Silicon app for macOS 12 or later. Build output is ignored by Git.

Run the app's five virtual CoreMIDI tests with:

```sh
bash midi-bridge/scripts/test.sh
```

Connect both USB devices to the Mac, open the app, and select `BeatStep Pro → Teensy MIDI` if automatic selection does not find them. See the [Mac app README](midi-bridge/README.md) for prerequisites, controls, signing, and test details. These app commands are separate from the firmware build below.

## Playing it

- **Tap:** a positive Note On starts one short pulse. Note Off and Note On with velocity zero never make a hit. Gate duration does not hold the coil on or truncate the tap. Velocity changes pulse width.
- **Flam:** a tap followed by a quieter, adjustable second tap. The pair continues after Note Off. A new Note On on that solenoid or All Off cancels the previous pair.
- **Buzz:** MIDI pitch sets the pulse repetition frequency; velocity and an ADSR envelope change pulse width. Matching Note Off enters release. The sequencer's gate length controls how long the envelope is held.
- Each solenoid keeps its own mode, envelope, tap, flam, duty, and fixed drum-buzz pitch settings. Settings are volatile and return to defaults on reboot; nothing is written to flash during playing.

These are physical actuators, so envelope and velocity affect the sound through pulse energy, not calibrated loudness. Very short pulses may not move a particular solenoid.

## BeatStep Pro setup

The generated [MCC preset and setup instructions](beatstep-pro/README.md) configure the Control Mode mappings below in one project import. The separate global settings currently have a JSON reference and manual checklist; a native Device Settings export still needs to be captured from MCC with the Pro connected.

Use Arturia MIDI Control Center to configure and store the following on the Pro. Set its clock source to **Internal** for this setup. Route its USB MIDI clock/transport through MIDI Bridge so Stop and the clock-loss timeout reach the instrument; verify them before powered testing.

| Pro section | MIDI channel | Purpose |
| --- | --- | --- |
| Sequencer 1 | 1 | Any MIDI note plays solenoid 1 at that pitch in buzz mode |
| Sequencer 2 | 2 | Any MIDI note plays solenoid 2 at that pitch in buzz mode |
| Drum sequencer | 10 | Four banks of solenoid articulations, below |
| Control Mode pads / encoders | 16 | Select a solenoid and edit its parameters while sequences play |

Channels 3 and 4 also address solenoids 3 and 4 directly if you reassign a melodic sequencer or use another MIDI source. MIDI channel numbers here are **1-based**.

For a first pattern, use Sequencers 1 and 2 for two pitched buzz lines, plus drum pads 3 and 4 for taps on solenoids 3 and 4. Alternatively, use drum pads 1–4 for four independent tap patterns. In Control Mode you can edit a selected voice without stopping playback; Control Mode CC movements are live edits, not an extra CC automation sequencer.

### Drum pads: channel 10

Set the 16 drum pads to MIDI notes **36–51 in order**. Use the numeric notes rather than octave labels, which vary between applications.

| Pro pads | MIDI notes | Solenoids | Action |
| --- | --- | --- | --- |
| 1–4 | 36–39 | 1–4 | Explicit single tap |
| 5–8 | 40–43 | 1–4 | Explicit flam |
| 9–12 | 44–47 | 1–4 | Explicit buzz at that solenoid's CC28 pitch |
| 13–16 | 48–51 | 1–4 | Use that solenoid's selected Tap/Buzz mode and flam setting |

This gives you sequenced articulations without needing to record mode CCs: for example, put normal hits for solenoid 3 on pad 3 and its flams on pad 7. Buzz pads respond to their own Note Off, so drum gate length matters for those pads.

**One solenoid can only play one articulation at a time.** The latest Note On wins, including when drum and melodic parts share a solenoid. Earlier Note Off from a different note or channel cannot stop the newer buzz. Repeated same-pitch overlapping notes are inherently ambiguous in MIDI 1.0; use normal non-overlapping gates for them. Multiple events for the same actuator arriving in one timer batch may collapse to the latest event. Very fast rolls can also lose hits to the physical limits below.

### Control Mode pads

Configure these as MIDI **CC** on channel **16**, sent over USB. Momentary pads use On=127, Off=0; selection/mode/reset actions ignore the release value.

| Control pad | CC | Behavior |
| --- | --- | --- |
| 1–4 | 110–113 | Select solenoid 1–4 for editing |
| 5 | 114 | Set selected solenoid to Tap |
| 6 | 115 | Set selected solenoid to Buzz |
| 7 | 18 | Flam on/off; use Toggle mode, values 127/0 |
| 15 | 119 | Reset selected solenoid's settings and stop it |
| 16 | 120 | All Off immediately, for all solenoids |

The flam-toggle pad's LED is controller-local: selecting another solenoid does not update the LED from the firmware. Use explicit CC18=0 or 127 if you need a known state. No MIDI feedback or automatic preset upload is implemented.

### Control Mode encoders

Configure encoders 1–9 as CC **70–78**, channel **16**, **Relative 1**. This lets you select another solenoid and turn a knob without jumping its stored value. The firmware accepts Arturia Relative 1 values 61–63 (decrement 3–1), 65–67 (increment 1–3), and ignores neutral/unsupported values. Do not use Absolute or another relative format with these CCs.

| Encoder | Relative CC | Absolute CC equivalent | Parameter | Range / default |
| --- | --- | --- | --- | --- |
| 1 | 70 | 20 | Attack | 0–2 s, quadratic; ~1.1 ms |
| 2 | 71 | 21 | Decay | 0–2 s, quadratic; ~31.7 ms |
| 3 | 72 | 22 | Sustain | 0–100%; ~71% |
| 4 | 73 | 23 | Release | 0–2 s, quadratic; ~17.9 ms |
| 5 | 74 | 24 | Tap width at velocity 127 | 1–10 ms; ~4 ms |
| 6 | 75 | 25 | Flam interval, start to start | 12–120 ms; ~35 ms |
| 7 | 76 | 26 | Second tap strength | 0–100% of first pulse; ~60% |
| 8 | 77 | 27 | Buzz duty at full envelope/velocity | 1–25%; ~15% |
| 9 | 78 | 28 | Fixed pitch for drum buzz pads | MIDI note 0–127; 45 (A2) |

The displayed ranges are request ranges; output quantization, minimum off-time and the energy budget can shorten or suppress pulses. Envelope times update at 1 kHz, GPIO at 20 kHz. The pulse repetition frequency is clamped to **20–500 Hz**, regardless of the MIDI pitch. A4 is MIDI 69 / 440 Hz, correcting the original sketch's octave offset.

Absolute CCs 20–28 accept standard 0–127 values, useful for a DAW, script, or encoders explicitly configured in Absolute mode. All these parameter CCs also work on channels 1–4, directly addressing that solenoid instead of the current selection.

Additional messages:

- CC16 on channel 16, value 0–3: select solenoid 1–4.
- CC17 on channel 16 or 1–4: 0–63 Tap, 64–127 Buzz. A mode change stops that voice. Defaults to Buzz.
- CC18 on channel 16 or 1–4: 0–63 flam disabled, 64–127 enabled. Defaults off; applies to configured-mode taps, not explicit single-tap pads.
- CC120 / CC123: stop one solenoid on channels 1–4; stop all on channels 10 or 16. Value is ignored. This supports MIDI Bridge's All Off cleanup.
- MIDI Stop / System Reset: cancel everything, including releases and pending flams.
- MIDI Start / Continue: clear voices and arm a 500 ms clock-loss timeout. MIDI Clock refreshes it. After panic/Stop/timeout, free playing works without clock; a new Start/Continue re-arms the watchdog.
- Other note channels, notes outside 36–51 on channel 10, and unsupported CCs/messages are ignored. Pitch bend, pressure, sustain pedal, SysEx configuration, and MIDI output are not implemented.

## Pulse limits and failure handling

The initial constants in `solenoid_engine.h` cap each pulse at **10 ms**, require **1 ms off** between pulses, limit long-term commanded duty to **25%** with a small startup energy allowance, and stop a held buzz after **5 seconds**. Repeated Note On, settings reset, and panic do not replenish the energy allowance. Budget-limited hits are dropped; hits arriving while a coil is on are retriggered only after the off interval. These are initial software bounds, **not measured thermal limits for your coils or driver**.

The timer drives outputs low on USB deconfiguration, a MIDI-queue overflow, a main-loop heartbeat stall over 250 ms, a timer gap over 200 µs, or the armed clock timeout. Outputs start low; timer-allocation failure leaves them low. A timer or CPU that stops executing cannot enforce software limits, and USB suspend behavior is host/core dependent. Use the existing appropriately rated switching driver, flyback protection and separate solenoid supply; never connect a coil directly to GPIO. Verify the actual coil voltage, driver ratings and acceptable duty before powered testing.

The timer owns all voice state. Main-loop MIDI callbacks enqueue bounded events with interrupts briefly masked. At most eight events are processed per 50 µs timer tick; queue overflow discards the pending queue and silences the instrument. No dynamic allocation, floating point, USB calls or blocking waits run in the timer.

## Build and validate

Host tests (Clang or GCC, with AddressSanitizer and UndefinedBehaviorSanitizer):

```sh
bash scripts/test.sh
```

Validation on October 7, 2026: **14 engine tests and 4 adapter tests pass** with AddressSanitizer/UndefinedBehaviorSanitizer. Coverage includes tap leading edges, flam timing/cancellation, ADSR, channel/note routing, stale Note Off, relative controls, pitch, timer rollover, clock loss, duty limits under retriggering, and randomized MIDI events. Adapter tests use stubbed GPIO/USB/timer APIs to check FIFO/ring wrap, queue overflow, disconnect, main-loop stall, and transport/panic. They do not exercise real USB or hardware interrupt timing.

Arduino CLI 1.5.1 with Teensy core 1.62.0 successfully compiles both MIDI and Serial + MIDI at 72 MHz. MIDI uses 14,660 bytes flash / 3,836 bytes static RAM; Serial + MIDI uses 14,388 / 4,908 bytes. No physical device has been flashed or tested as part of this validation.

Install Arduino CLI and the official Teensy package, then compile:

```sh
arduino-cli core update-index --additional-urls https://www.pjrc.com/teensy/package_teensy_index.json
arduino-cli core install teensy:avr@1.62.0 --additional-urls https://www.pjrc.com/teensy/package_teensy_index.json
bash scripts/build-teensy.sh
```

The script stages the sketch in `.build/sketch/solenoids` because Arduino requires the sketch folder to match the `.ino` name. Default target: `teensy:avr:teensy31:usb=midi,speed=72,opt=o2std` (72 MHz, without overclocking). `ARDUINO_CLI` and `ARDUINO_CONFIG` optionally select your CLI binary and YAML configuration. `FQBN` can select the tested `usb=serialmidi` variant. The script builds only; it never uploads. Firmware output is `.build/firmware/solenoids.ino.hex`.

For Arduino IDE, open the staged sketch, select **Teensy 3.2 / 3.1**, **USB Type: MIDI** (or **Serial + MIDI**), and **CPU Speed: 72 MHz**. Upload with Teensy Loader only after confirming the MCU and disconnecting the solenoid supply for initial checks.

Before the jam, verify on the physical instrument:

1. With the solenoid supply disconnected, confirm USB enumeration and pins 9–12 idle low. Check pulse widths and duty with a scope or logic analyzer.
2. Test one connected solenoid at a time with a suitable current-limited supply and short, low-energy taps. Confirm pin order and calibrate the software constants to the actual coil/driver.
3. Check drum pads 1–4 produce separate taps, pads 5–8 make flams, and the two melodic sequences buzz independently. Exercise mode changes, velocity, gate length, ADSR, and relative encoders.
4. Check Stop, MIDI Bridge All Off, cable disconnect/reconnect, laptop sleep, and stalled clock leave the outputs off. Reconnect must not replay old hits.
5. Check sustained/retriggered playing and coil/driver temperatures within the component ratings. No physical timing, temperature or listening validation has been performed by the automated tests.

## References

- [Arturia BeatStep Pro manual](https://downloads.arturia.net/products/beatstep-pro/manual/beatstep-pro_Manual_2_0_EN.pdf): MIDI Control Center, Control Mode, drum note mapping, clock/transport.
- [Arturia Relative 1 documentation](https://downloads.arturia.com/products/minilab-mkII/manual/MiniLabmkII_Manual_1_0_7_EN.pdf): encoder values 61–63 and 65–67.
- [PJRC USB MIDI](https://www.pjrc.com/teensy/td_midi.html) and [IntervalTimer](https://www.pjrc.com/teensy/td_timing_IntervalTimer.html).
