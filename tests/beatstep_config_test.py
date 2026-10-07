"""Check the exported preset's MIDI contract and preservation of user patterns."""

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("preset", ROOT / "scripts/generate-beatstep-pro-config.py")
preset = importlib.util.module_from_spec(spec)
spec.loader.exec_module(preset)


class ConfigTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.project = preset.blank_project()

    def test_full_project_is_silent(self):
        self.assertEqual(len(self.project), 75527)
        steps = {k: v for k, v in self.project.items() if "_105_" in k}
        self.assertEqual(len(steps), (2 + 16) * 16 * 64)
        self.assertEqual(set(steps.values()), {0})
        for seq in (16, 21, 26):
            for pattern in range(1, 17):
                self.assertEqual(self.project[f"{seq}_99_{pattern}"], 16)

    def test_encoder_contract(self):
        for index, cc in enumerate(range(70, 79)):
            item = 32 + index
            for param, value in {1: 1, 2: 15, 3: cc, 4: 0, 5: 127, 6: 1, 7: 0, 8: 1}.items():
                self.assertEqual(self.project[f"{item}_{param}"], value)

    def test_pad_contract(self):
        for pad, cc in {1: 110, 2: 111, 3: 112, 4: 113, 5: 114, 6: 115,
                        7: 18, 15: 119, 16: 120}.items():
            item = 111 + pad
            for param, value in {1: 8, 2: 15, 3: cc, 4: 0, 5: 127,
                                 6: 0 if pad == 7 else 1, 8: 1}.items():
                self.assertEqual(self.project[f"{item}_{param}"], value)

    def test_unused_controls_disabled(self):
        for item in (*range(41, 48), *range(119, 126), *range(48, 64)):
            self.assertEqual(self.project[f"{item}_1"], 0)

    def test_preserves_existing_project_and_metadata(self):
        original = dict(self.project)
        original.update({"version": "2.0.1", "16_105_1_1": 1, "16_104_1_1": 45,
                         "26_105_2_3_4": 1, "68_82_1_1_1": 2, "90_110": 80,
                         "32_3": 99, "future_parameter": 123})
        result = preset.configure_existing(original)
        self.assertEqual(result["32_3"], 70)
        self.assertEqual(original["32_3"], 99)
        mappings = preset.controller_map()
        self.assertEqual({k: v for k, v in result.items() if k not in mappings},
                         {k: v for k, v in original.items() if k not in mappings})
        for bad in ({"device": "BeatStep"}, {}, []):
            with self.assertRaises(ValueError):
                preset.configure_existing(bad)

    def test_checked_in_export_matches_generator(self):
        self.assertEqual(preset.DESTINATION.read_text(), preset.render(self.project))

    def test_vendor_trailing_comma_without_changing_strings(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "example.beatsteppro"
            path.write_text('{"device":"BeatStepPro", "metadata":"keep,}", "32_1":1,}')
            self.assertEqual(preset.load_project(path),
                             {"device": "BeatStepPro", "metadata": "keep,}", "32_1": 1})

    def test_global_reference_matches_firmware_routing(self):
        reference = json.loads((ROOT / "beatstep-pro/device-settings.json").read_text())
        self.assertFalse(reference["mcc_importable"])
        values = {row["globalParamId"]: row["raw"] for row in reference["settings"]}
        for param, value in {6: 15, 64: 0, 66: 1, 68: 9, 39: 3, 32: 0, 35: 14,
                             96: 1, 97: 15, 100: 120}.items():
            self.assertEqual(values[param], value)


if __name__ == "__main__":
    unittest.main()
