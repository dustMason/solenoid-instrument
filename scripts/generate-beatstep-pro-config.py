#!/usr/bin/env python3
"""Generate an offline MCC project; never opens MIDI ports or writes a device.

Parameter IDs and blank-pattern layout were checked against BeatStepPro.json
and Factory/Default.beatsteppro in Arturia MIDI Control Center 1.23.0.134.
See beatstep-pro/README.md for provenance and separate global settings.
"""

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DESTINATION = ROOT / "beatstep-pro" / "Tapper-Buzzer.beatsteppro"


def controller_map():
    """Explicit channel 16 and USB, independent of the global User Channel."""
    result = {}
    for index in range(16):
        # Knob mode: 0=Off, 1=CC. Data mode 1=Arturia Relative #1.
        knob = {1: 1 if index < 9 else 0, 2: 15, 3: 70 + index if index < 9 else 0,
                4: 0, 5: 127, 6: 1, 7: 0, 8: 1}
        # Pad mode: 0=Off, 8=CC. Behavior: 0=Toggle, 1=Gate.
        pad_ccs = {0: 110, 1: 111, 2: 112, 3: 113, 4: 114, 5: 115,
                   6: 18, 14: 119, 15: 120}
        pad = {1: 8 if index in pad_ccs else 0, 2: 15, 3: pad_ccs.get(index, 0),
               4: 0, 5: 127, 6: 0 if index == 6 else 1, 8: 1}
        # Disable CCs only in Control Mode; step sequencing still works normally.
        step_button = {1: 0, 2: 15, 3: 0, 4: 0, 5: 127, 6: 1, 8: 1}
        for item, parameters in ((32 + index, knob), (112 + index, pad),
                                 (48 + index, step_button)):
            result.update({f"{item}_{param}": value for param, value in parameters.items()})
    return result


def blank_project():
    """Complete 16-pattern project: every sequencer step off, 120 BPM."""
    project = {"device": "BeatStepPro"}
    project.update(controller_map())
    project.update({f"90_{param}": value for param, value in
                    {100: 50, 102: 0, 103: 0, 109: 0, 110: 93, 111: 96}.items()})
    for pattern in range(1, 17):
        for sequencer in (16, 21, 26):
            settings = {99: 16, 97: 0, 98: 2, 100: 50, 101: 0, 102: 0, 103: 0}
            settings.update({80: 0} if sequencer == 26 else {81: 0, 96: 60})
            for param, value in settings.items():
                project[f"{sequencer}_{param}_{pattern}"] = value
            tracks = range(1, 17) if sequencer == 26 else (None,)
            for track in tracks:
                suffix = str(pattern) if track is None else f"{pattern}_{track}"
                if track is not None:
                    project[f"26_99_{suffix}"] = 16
                for step in range(1, 65):
                    for param, value in {104: 50 if track else 60, 105: 0,
                                         106: 100, 107: 50}.items():
                        project[f"{sequencer}_{param}_{suffix}_{step}"] = value
        for sequencer_slot in range(1, 4):
            for pattern_slot in range(1, 17):
                project[f"68_82_{pattern}_{sequencer_slot}_{pattern_slot}"] = 127
    return project


def configure_existing(project):
    """Replace only Control Mode mappings in an existing MCC export."""
    if not isinstance(project, dict) or project.get("device") != "BeatStepPro":
        raise ValueError("Expected a BeatStepPro project exported by MIDI Control Center")
    result = dict(project)
    result.update(controller_map())
    return result


def render(project):
    return json.dumps(project, indent=2, sort_keys=True) + "\n"


def load_project(path):
    # MCC's bundled template has a trailing comma. Leave quoted strings intact.
    source = re.sub(r'("(?:\\.|[^"\\])*")|,\s*(?=[}\]])',
                    lambda match: match.group(1) or "", path.read_text())
    return json.loads(source)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-project", type=Path,
                        help="Preserve patterns/scenes in this MCC project; replace its Control Mode map")
    parser.add_argument("--output", type=Path, help="Output .beatsteppro path")
    parser.add_argument("--check", action="store_true", help="Verify output is current without writing")
    args = parser.parse_args()
    if args.base_project and not args.output:
        parser.error("--base-project requires --output (keep the checked-in blank preset unchanged)")
    destination = args.output or DESTINATION
    if destination.suffix != ".beatsteppro":
        parser.error("--output must end in .beatsteppro")
    if args.base_project and args.base_project.resolve() == destination.resolve():
        parser.error("Choose a different output path to preserve your original export")
    try:
        project = (configure_existing(load_project(args.base_project))
                   if args.base_project else blank_project())
        text = render(project)
        if args.check:
            if not destination.exists() or destination.read_text() != text:
                parser.exit(1, f"Out of date: {destination}\n")
            print(f"Current: {destination}")
        else:
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_text(text)
            print(f"Wrote {destination} ({len(project):,} entries)")
    except (OSError, ValueError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
