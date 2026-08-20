import argparse
import json
import unittest
from pathlib import Path

from webkitcorepy import OutputCapture

from webkitsysprof import summary, dump, analyze

SAMPLE_CAPTURE_FILE = str(Path(__file__).parent / "assets" / "sample.syscap")


def _capture_stdout(func, args):
    with OutputCapture() as captured:
        func(args)
    return captured.stdout.getvalue()


class SubcommandsTest(unittest.TestCase):
    def test_summary(self):
        args = argparse.Namespace(capture_file=SAMPLE_CAPTURE_FILE)
        stdout = _capture_stdout(summary.summary, args)

        self.assertIn(f"File: {SAMPLE_CAPTURE_FILE}", stdout)
        self.assertIn("Marks: 669", stdout)
        self.assertIn("Counters: 53", stdout)

    def test_dump_marks_csv(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, marks=True, counters=False, format="csv"
        )
        stdout = _capture_stdout(dump.dump, args)

        stdout_lines = stdout.split("\n")
        self.assertEqual(len(stdout_lines), 669 + 1 + 1)

    def test_dump_counters_csv(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, marks=False, counters=True, format="csv"
        )
        stdout = _capture_stdout(dump.dump, args)

        stdout_lines = stdout.split("\n")
        self.assertEqual(len(stdout_lines), 540 + 1 + 1)

    def test_dump_marks_json(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, marks=True, counters=False, format="json"
        )
        stdout = _capture_stdout(dump.dump, args)

        marks = json.loads(stdout)
        self.assertEqual(len(marks), 669)
        self.assertEqual(
            set(marks[0].keys()),
            {"group", "name", "message", "time", "duration", "end_time"},
        )

    def test_dump_counters_json(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, marks=False, counters=True, format="json"
        )
        stdout = _capture_stdout(dump.dump, args)

        counter_values = json.loads(stdout)
        self.assertEqual(len(counter_values), 540)
        self.assertEqual(
            set(counter_values[0].keys()),
            {"category", "name", "description", "time", "offset", "value"},
        )

    def test_analyze_text(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, format="text", timespan="-"
        )
        stdout = _capture_stdout(analyze.analyze, args)

        self.assertIn("Timespan: 0.0000 - 4.4673 [s]", stdout)
        self.assertIn("vblanks: 35", stdout)

    def test_analyze_json(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, format="json", timespan="-"
        )
        stdout = _capture_stdout(analyze.analyze, args)

        report = json.loads(stdout)
        self.assertEqual(int(report["document"]["timespan"]["begin"]), 0)
        self.assertEqual(int(report["document"]["timespan"]["end"]), 4467)
        self.assertEqual(report["rendering"]["vblanks"], 35)

    def test_analyze_json_statistics_cover_all_relevant_marks(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, format="json", timespan="-"
        )
        stdout = _capture_stdout(analyze.analyze, args)

        statistics = json.loads(stdout)["statistics"]
        self.assertEqual(set(statistics), set(analyze.MARKS_RELEVANT_FOR_STATISTICS))
        self.assertEqual(statistics["CompositingUpdate"]["duration"]["n"], 7)
        self.assertEqual(statistics["RenderTreeBuild"]["duration"]["n"], 3)
        # Tile geometry depends on the rendering backend, so only check that
        # the dirty area was extracted from every PaintTile message.
        self.assertEqual(statistics["PaintTile"]["dirty_pixels"]["n"], 80)
        self.assertGreater(statistics["PaintTile"]["dirty_pixels"]["min"], 0)

    def test_analyze_with_custom_timespan(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, format="text", timespan="0-0"
        )
        stdout = _capture_stdout(analyze.analyze, args)

        self.assertIn("Timespan: 0.0000 - 0.0000 [s]", stdout)
        self.assertIn("vblanks: 0", stdout)

    def test_analyze_with_custom_timespan_begin(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, format="text", timespan="500-"
        )
        stdout = _capture_stdout(analyze.analyze, args)

        self.assertIn("Timespan: 0.5000 - 4.4673 [s]", stdout)
        self.assertIn("vblanks: 25", stdout)

    def test_analyze_with_custom_timespan_end(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, format="text", timespan="-500"
        )
        stdout = _capture_stdout(analyze.analyze, args)

        self.assertIn("Timespan: 0.0000 - 0.5000 [s]", stdout)
        self.assertIn("vblanks: 10", stdout)


if __name__ == "__main__":
    unittest.main()
