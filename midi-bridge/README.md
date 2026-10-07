# MIDI Bridge

A small native Mac app that forwards the BeatStep Pro's USB MIDI to the Teensy solenoid instrument. Built with SwiftUI, AppKit and Apple's CoreMIDI. No third-party dependencies, account, network connection, audio driver, DAW or IAC setup.

## Build from source

Requirements:

- A Mac with Xcode Command Line Tools and Swift 5.9 or later. If the tools are missing, run `xcode-select --install` and finish the installer. Check the compiler with `xcrun swift --version`.
- macOS 12 or later to run the resulting app. The compiler itself may require a newer macOS version.

From the repository root:

```sh
bash midi-bridge/scripts/build-app.sh
open "midi-bridge/dist/MIDI Bridge.app"
```

Or, from this `midi-bridge` directory, run `bash scripts/build-app.sh`. The script resolves its own directory, so it works from either location.

The script builds a release executable with Swift Package Manager, packages it as `dist/MIDI Bridge.app`, signs it locally, verifies the signature, and creates `dist/MIDI Bridge.zip`. It does not launch the app, install anything globally, or flash the Teensy. There are no packages to download and no Apple developer account is required.

The binary targets the build Mac's architecture: building on the M1 MacBook Air produces an **arm64** app. This script does not produce a universal binary. The app is ad-hoc signed, not Developer ID notarized; a downloaded/transferred copy may require macOS's normal **Open Anyway** approval. Build caches, SwiftPM workspace state and `dist/` are ignored by Git.

Source layout:

- `Sources/BridgeCore/`: MIDI forwarding, endpoint selection, saved settings and connection state.
- `Sources/MIDIBridge/main.swift`: native window, controls, sleep handling and app lifecycle.
- `Tests/BridgeCoreTests/`: virtual CoreMIDI integration tests.
- `Package.swift`: Swift package and macOS deployment target.
- `scripts/build-app.sh`: release build, app bundle and ZIP packaging.
- `scripts/test.sh`: test runner with local compiler caches.

## Use it

1. Build the app above, or copy the resulting app/ZIP to the M1 MacBook Air.
2. Plug both the BeatStep Pro and Teensy into the Mac using USB **data** cables. Continue using the solenoid instrument's appropriate external power supply.
3. Open MIDI Bridge. It automatically selects a uniquely named `BeatStep Pro` input and `Teensy` or `Solenoid` output. If your device names differ, choose them from **From** and **To** once.
4. Press Play on the BeatStep Pro. The activity indicator confirms MIDI arriving at the app; it does not prove that the Teensy has actuated a solenoid.
5. Keep the app running and the laptop lid open. Closing the window quits the app and releases held notes. It does not install a login item or background service.

Device choices are remembered. Reconnecting the same device restores routing automatically. If CoreMIDI assigns a new ID, an exact, unique name match is accepted. The app does not silently substitute an unrelated device or guess between duplicate device names.

**Pause** and **All Off** stop forwarding, flush scheduled output, send Note Off for tracked held notes, and send sustain-off / All Notes Off / All Sound Off on all channels. **Resume** explicitly restarts routing. Disconnecting a source or quitting also sends these messages while the destination is reachable. This is best-effort MIDI cleanup, not a hardware power cutoff. The [firmware in this repository](../README.md#pulse-limits-and-failure-handling) supplies instrument-side pulse limits and timeout handling.

The app asks macOS to prevent idle sleep while open. Deliberate sleep/lid closure pauses routing; click Resume after waking.

## Firmware compatibility

This forwards standard MIDI without changing channel numbers, notes, velocities, CCs or timing. The [firmware and controller setup in the parent directory](../README.md#beatstep-pro-setup) implement tap/buzz/flam/ADSR, drum-channel routing and relative encoder controls. Flash that firmware before using the new mapping; the original sketch assumed channels 1–4 and did not bounds-check the output index.

The app uses CoreMIDI's MIDI 1.0 UMP interface and preserves incoming event lists and timestamps. Clock, transport, channel messages and SysEx are forwarded. CoreMIDI handles conversion to USB MIDI for the physical devices. Messages unsupported by the firmware are ignored there.

## Test

You can also [test the Teensy's firmware directly from the Mac](../docs/bench-test.md) before the BeatStep Pro arrives. That bench path uses SendMIDI and does not require this bridge.

From the repository root:

```sh
bash midi-bridge/scripts/test.sh
```

Integration tests need access to the logged-in Mac's CoreMIDI service. Run them in a normal local macOS session. A restricted execution sandbox without CoreMIDI access may prevent MIDI client creation. Tests create only their own virtual MIDI devices and use isolated temporary preferences; they never select physical instruments.

The five tests cover MIDI message/channel/timestamp preservation, large event lists, held-note cleanup and paused forwarding, device reconnection and saved pause state, and ambiguous/missing device selection. These are separate from the firmware's host tests (`bash scripts/test.sh` from the repository root).

Validation on October 7, 2026: the release build, app signature verification and all five tests passed from this repository on an Apple Silicon Mac using Swift 6.4. The packaged executable is arm64 with a macOS 12.0 deployment target. The CoreMIDI tests passed with access to the logged-in Mac's MIDI service; the restricted sandbox could not create MIDI clients.

Physical BeatStep Pro → MacBook Air → Teensy operation still needs testing, including cable reconnection and All Off. Virtual tests and successful compilation do not validate the solenoid hardware or audible timing.
