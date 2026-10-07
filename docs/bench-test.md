# Test the Teensy before the BeatStep arrives

Use **Mac → USB → Teensy**. [SendMIDI](https://github.com/gbevin/SendMIDI) generates the same notes and CCs the BeatStep will send. MIDI Bridge is not in this direct path; its automated virtual-MIDI tests can run separately. None of the hardware checks below has been performed yet.

## 1. Prepare and flash

Confirm the actual Teensy is a 3.1/3.2 and the driver inputs connect to pins 9–12. Keep the external solenoid supply **off/disconnected** through the electrical checks. [Build the firmware](../README.md#build-and-validate) and load `.build/firmware/solenoids.ino.hex` with Teensy Loader, then connect USB to the Mac. Use a scope/logic analyzer at the Teensy's logic outputs and logic ground; do not connect a logic-level analyzer to the coil supply or switched coil terminals.

Install SendMIDI using its documented Homebrew command, then list the outputs:

```sh
brew install gbevin/tools/sendmidi
sendmidi list
```

Copy the Teensy's **full output name** from that list (including any duplicate-number suffix). SendMIDI falls back to a partial-name match, so avoid abbreviating it. In the same Terminal session:

```sh
TAPPER_MIDI='Teensy MIDI'  # Replace with the full name from your list.
sendmidi dev "$TAPPER_MIDI" ch 16 cc 120 0
```

Pins 9–12 should be low at idle. Keep that last command handy: it stops all voices. Removing coil power is the physical stop if USB/software is unresponsive.

## 2. Verify tap and flam outputs

Run each command separately and observe pin 9. MIDI channels here are one-based; notes are numeric. A successful shell command only means MIDI was sent, not that the device passed the check.

**Single tap, even with immediate Note Off:**

```sh
sendmidi dev "$TAPPER_MIDI" ch 1 cc 24 0 ch 10 on 36 127 off 36 0
```

Expect exactly one approximately **1 ms** pulse. Note Off must not truncate it. The explicit drum tap works regardless of the voice's current Tap/Buzz mode.

**Zero velocity and Note Off must not tap:**

```sh
sendmidi dev "$TAPPER_MIDI" ch 10 on 36 0 off 36 0
```

Expect no pulse. For the other outputs, repeat the first test with configuration channel/note **2/37 → pin 10**, **3/38 → pin 11**, and **4/39 → pin 12**. Only the addressed output should move.

**Flam:**

```sh
sendmidi dev "$TAPPER_MIDI" ch 1 cc 24 0 cc 25 0 cc 26 64 ch 10 on 40 127 off 40 0
```

Expect two pulses: approximately **1 ms** then **0.5 ms**, with **12 ms between their leading edges**. Allow one 50 µs timer tick of quantization. Wait at least a second between these manual tests so the energy budget has recovered.

## 3. Verify buzz and stop behavior, still with coil power off

This sets voice 1 to Buzz, instantaneous attack/decay/release, full sustain, and the minimum 1% requested duty. Note 45 is 110 Hz:

```sh
sendmidi dev "$TAPPER_MIDI" ch 16 cc 120 0 ch 1 cc 17 127 cc 20 0 cc 21 0 cc 22 127 cc 23 0 cc 27 0 on 45 127 +00.100 off 45 0
```

Expect about 100 µs high every 9.1 ms, stopping within roughly 1 ms after Note Off. SendMIDI's `+00.100` means a **100 ms delay**; these are not realtime host timing guarantees.

With that configuration, run these separately:

| Check | Command after `sendmidi dev "$TAPPER_MIDI"` | Expected on pin 9 |
| --- | --- | --- |
| All Off | `ch 1 on 45 127 +00.100 ch 16 cc 120 0` | Pulses end on All Off, without needing Note Off |
| Realtime Stop | `ch 1 on 45 127 +00.100 stop` | Pulses end on Stop |
| Lost clock | `start ch 1 on 45 127 +00.700 off 45 0 stop` | Start arms the watchdog; with no clock messages, output ends after about 500 ms |
| Held-note ceiling | `ch 16 cc 120 0 ch 1 on 45 127 +05.200 off 45 0` | Even without Note Off, output ends after about 5 s |

For ADSR, use 250 ms attack/decay/release, half sustain, and about 5.5% requested duty:

```sh
sendmidi dev "$TAPPER_MIDI" ch 1 cc 20 45 cc 21 45 cc 22 64 cc 23 45 cc 27 24 on 45 127 +00.800 off 45 0 +00.300 ch 16 cc 120 0
```

Pulse widths should rise, fall to half, then fade after Note Off; the repetition frequency stays near 110 Hz. Check USB disconnect/reconnect during a buzz with coil power off: outputs must settle low and reconnection must not replay an old hit. Testing physical USB behavior requires the real device; the host tests do not cover it.

## 4. Try one powered coil

Once you have confirmed the individual coil voltage, driver/flyback protection, and supply rating, use a suitably current-limited supply and connect **one coil**. Run only the **single 1 ms tap** from section 2 initially. A pulse this short may not move it. Increase `cc 24` gradually if needed: values **0 / 14 / 28 / 42** request approximately **1 / 2 / 3 / 4 ms** at velocity 127. Record the lowest reliable width before testing the other outputs. Restore the appropriate per-voice setting when switching pins.

Establish acceptable duty and component temperatures before running flams, buzzes, or repeated patterns with coil power on. The firmware's 25% limit is a software ceiling, not a verified rating for your solenoids. If you cannot measure pulse timing yet, record that check as pending.

## 5. Finish and record results

The commands above change volatile voice settings. With coil power off, restore all four voices to the jam defaults (or disconnect/reconnect Teensy power):

```sh
sendmidi dev "$TAPPER_MIDI" ch 16 cc 120 0 ch 1 cc 119 127 ch 2 cc 119 127 ch 3 cc 119 127 ch 4 cc 119 127
```

| Item | Result / measurement |
| --- | --- |
| Teensy model, firmware commit, USB name | Pending |
| Coil voltage, driver/supply ratings | Pending |
| Pins 9–12 idle low; separate taps; zero-velocity silence | Pending |
| Measured tap width / flam gap / buzz period | Pending |
| Note Off, All Off, Stop, clock timeout, 5 s ceiling | Pending |
| USB disconnect/reconnect without replay | Pending |
| Lowest reliable tap width per coil; duty/temperature limits | Pending |

Once the BeatStep arrives, continue with the [preset import](../beatstep-pro/README.md) and the [MIDI Bridge connection](../midi-bridge/README.md#use-it). The remaining integration checks are pad/knob mappings, simultaneous sequences, bridge Pause/All Off, and laptop sleep/reconnect.
