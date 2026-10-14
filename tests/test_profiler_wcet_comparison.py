"""Regression tests for the profiler/WCET comparison command."""
import csv
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[2] / "sewcet" / "tools" / "compare_csv.py"


class ProfilerWcetComparisonTests(unittest.TestCase):
    def compare(self, measured, bounds, functions=None, extra_args=()):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            for name, records in (("measured.csv", measured), ("static.csv", bounds)):
                with (root / name).open("w", newline="", encoding="utf-8") as output:
                    writer = csv.writer(output)
                    writer.writerow(["function", "max_cycles"])
                    writer.writerows(records)
            config = root / "wcet_measure.json"
            config.write_text(json.dumps({
                "functions": functions or [],
                "captures": {"target": "measured.csv"},
                "output_csv": "static.csv",
                "comparison_csv": "comparison.csv",
            }), encoding="utf-8")
            result = subprocess.run(
                [sys.executable, "-B", str(SCRIPT), "--config", str(config), *extra_args],
                capture_output=True, text=True, check=False,
            )
            with (root / "comparison.csv").open(newline="", encoding="utf-8") as output:
                rows = list(csv.DictReader(output))
            return result, rows

    def test_passing_and_equal_bounds_succeed(self):
        result, rows = self.compare([("lower", 99), ("equal", 100)],
                                    [("lower", 100), ("equal", 100)])
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertEqual(["OK", "OK"], [row["status"] for row in rows])

    def test_violation_fails_after_writing_all_results(self):
        result, rows = self.compare([("pass", 99), ("fail", 101)],
                                    [("pass", 100), ("fail", 100)])
        self.assertEqual(1, result.returncode, result.stderr)
        self.assertEqual(["OK", "VIOLATION"], [row["status"] for row in rows])
        self.assertIn("VIOLATION=1", result.stdout)

    def test_pendsv_composition_preserved(self):
        functions = [{"name": "delay", "switch_overhead": "PendSV_Handler"}]
        for measured, expected in ((1699, 0), (1835, 1)):
            with self.subTest(measured=measured):
                result, rows = self.compare(
                    [("delay", measured)], [("delay", 990), ("PendSV_Handler", 709)],
                    functions,
                )
                self.assertEqual(expected, result.returncode, result.stderr)
                self.assertEqual("1699", rows[0]["wcet_cycles"])

    def test_static_only_and_missing_bound_statuses_preserved(self):
        result, rows = self.compare(
            [("unanalysed", 10)], [("unmeasured", 20)], extra_args=("--all",),
        )
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertEqual(["NO_WCET", "STATIC_ONLY"], [row["status"] for row in rows])


if __name__ == "__main__":
    unittest.main()
