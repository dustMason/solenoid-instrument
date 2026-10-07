# BeatStep Pro preset

[`Tapper-Buzzer.beatsteppro`](Tapper-Buzzer.beatsteppro) is the project to import into Arturia MIDI Control Center (MCC). It contains our Control Mode mapping, **empty patterns** in all three sequencers, and a 120 BPM project tempo. No step is enabled; you create the rhythms on the Pro.

[`device-settings.json`](device-settings.json) records the separate global settings. **It is a reference file, not an MCC import file.** Native `.beatsteppro_ds` serialization has not been confirmed, so those settings currently require the short manual setup below. Export them from MCC afterward to create a reusable native settings file.

## Import the project

1. Install [Arturia MIDI Control Center](https://www.arturia.com/support/downloads-manuals/product/beatstep-pro), connect the Pro by USB, and select **BeatStep Pro**. For the initial setup, leave the solenoid power supply disconnected.
2. Back up the Pro's existing project and Device Settings using MCC's **Recall From / Export** and separate **Device Settings → Export** controls.
3. Under **Local Templates**, click **Import** and choose `Tapper-Buzzer.beatsteppro`.
4. Select the imported template and an **unused destination project** under Device Memories, then **Store To**. This transfers the entire project, including its empty patterns; it replaces anything already in that destination. Select that project on the Pro afterward.
5. Apply the global settings below. They are device-wide and are not included in the project template.

To retain an existing project's sequences, first export that project and use the generator's `--base-project` option below. It replaces only the Control Mode assignments and preserves sequence, scene, tempo, version, and unknown metadata fields. Keep the original export as a backup.

## Global settings and front panel

In MCC's **Device Settings**, set the following. Channel numbers in this table are the visible, one-based numbers.

| Setting | Value |
| --- | --- |
| User Channel | 16 |
| Seq1 Send / Rcv MIDI Channel | 1 / 1 |
| Seq2 Send / Rcv MIDI Channel | 2 / 2 |
| Drum Send / Rcv MIDI Channel | 10 / 10 |
| Drum Map | Chromatic (pads 1–16 send notes 36–51) |
| Auto-sync | Off |
| Tempo | Project |
| Metronome On/Off | Off |
| Metronome Channel | 15 |
| Transports | MIDI |
| Stop Channel / Stop CC | 16 / 120 |

The metronome's channel 15 is ignored by this firmware if it is accidentally enabled. Stop CC120 provides an additional All Off message on top of MIDI realtime Stop. The Transports setting selects MIDI CC versus MMC commands; it is **not** a MIDI clock enable switch.

On the Pro, choose **SYNC → INT**. In **CONTROL MODE**, use the **KNOBS** button to select **CC**, not MCU/HUI. These front-panel selections are not set by the project file. Arturia's manual describes the Pro as sending MIDI clock and transport while it is the internal-clock master; verify these messages through the bridge before powered testing.

MCC applies global edits to the connected device directly. Once checked, use **Device Settings → Export** to save your own `Tapper-Buzzer.beatsteppro_ds`. That native file can later be imported separately from the project. It will contain the device's other global settings too.

## Controls at a glance

In **Control Mode**, all assigned controls send on channel 16 over USB. Their channel is explicit, so changing the global User Channel does not silently redirect them.

| Pads | Action |
| --- | --- |
| 1–4 | Select solenoid 1–4 |
| 5 / 6 | Selected solenoid: Tap / Buzz |
| 7 | Toggle flam for configured-mode taps |
| 15 | Reset selected solenoid's settings and stop it |
| 16 | Stop all solenoids |

Encoders **1–9** are **Arturia Relative #1**, CC70–78, slow acceleration: attack, decay, sustain, release, tap width, flam interval, flam strength, buzz duty, drum-buzz pitch. Pads 8–14, encoders 10–16, and the 16 step buttons are disabled **only in Control Mode**. Sequencer step editing remains available normally.

The flam toggle LED does not follow the firmware's selected voice. It reflects the pad's local toggle state; the firmware does not send LED feedback.

In **Drum mode**, the chromatic drum map gives:

| Pads | Articulation |
| --- | --- |
| 1–4 | Single taps on solenoids 1–4 |
| 5–8 | Flams on solenoids 1–4 |
| 9–12 | Fixed-pitch buzz on solenoids 1–4 |
| 13–16 | Selected Tap/Buzz mode for each solenoid |

SEQ1 and SEQ2 play pitched notes on solenoids 1 and 2. The latest note wins when a drum and melodic part address the same solenoid. See the [firmware README](../README.md) for the full MIDI contract.

## Regenerate or retain existing patterns

Run from the repository root with Python 3; no third-party packages are needed:

```sh
python3 scripts/generate-beatstep-pro-config.py
python3 scripts/generate-beatstep-pro-config.py --check
python3 tests/beatstep_config_test.py
```

To update only the controls in an existing MCC project export:

```sh
python3 scripts/generate-beatstep-pro-config.py \
  --base-project /path/to/My-Project.beatsteppro \
  --output /path/to/My-Project-Tappers.beatsteppro
```

The generator accepts MCC's trailing-comma JSON variant. It refuses a different device type and an output path matching the original export. It never opens MIDI ports or transmits to hardware.

## Validation and provenance

The parameter IDs, numeric values, and full 75,527-entry project layout were checked against **Arturia MIDI Control Center 1.23.0.134** for macOS, downloaded from [Arturia's official package](https://dl.arturia.net/products/mccu/soft/MIDI_Control_Center__1_23_0_134.pkg). Reference files inside that installer:

- `Library/Arturia/MIDI Control Center/Resources/BeatStepPro.json`
- `Library/Arturia/MIDI Control Center/Templates/BeatStepPro/Factory/Default.beatsteppro`

The generator constructs the project itself; the vendor's schema and template are not required at runtime or included in this repository. All non-controller keys and values match the vendor's empty factory project. The eight config tests verify the maps, empty sequences, disabled unused controls, preservation of existing patterns, and reproducible output.

**Pending:** MCC import/round-trip and hardware MIDI verification. The extracted MCC app opened locally but could not load its device resources without an installed resource directory, so this was a schema-level check, not a successful UI import. No hardware settings were changed. Native Device Settings file generation remains pending an actual MCC export.

References: [Arturia's explanation of the project schema](https://forum.arturia.com/t/trying-yo-understand-the-beatsteppro-file-schema-exported-from-mcc/2725), [BeatStep Pro manual](https://downloads.arturia.net/products/beatstep-pro/manual/beatstep-pro_Manual_2_0_EN.pdf), and [MIDI Control Center manual](https://downloads.arturia.com/products/mcc/manual/MIDI_Control_Center_Manual_1_0_EN.pdf).
