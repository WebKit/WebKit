import argparse
import json
from pathlib import Path

from webkitsysprof import summary, dump, analyze

SAMPLE_CAPTURE_FILE = str(Path(__file__).parent / "assets" / "sample.syscap")


def test_summary(capsys):
    args = argparse.Namespace(capture_file=SAMPLE_CAPTURE_FILE)
    summary.summary(args)

    stdout = capsys.readouterr().out
    assert f"File: {SAMPLE_CAPTURE_FILE}" in stdout
    assert "Marks: 669" in stdout
    assert "Counters: 53" in stdout


def test_dump_marks_csv(capsys):
    args = argparse.Namespace(
        capture_file=SAMPLE_CAPTURE_FILE, marks=True, counters=False, format="csv"
    )
    dump.dump(args)

    stdout_lines = capsys.readouterr().out.split("\n")
    assert len(stdout_lines) == 669 + 1 + 1


def test_dump_counters_csv(capsys):
    args = argparse.Namespace(
        capture_file=SAMPLE_CAPTURE_FILE, marks=False, counters=True, format="csv"
    )
    dump.dump(args)

    stdout_lines = capsys.readouterr().out.split("\n")
    assert len(stdout_lines) == 540 + 1 + 1


def test_dump_marks_json(capsys):
    args = argparse.Namespace(
        capture_file=SAMPLE_CAPTURE_FILE, marks=True, counters=False, format="json"
    )
    dump.dump(args)

    marks = json.loads(capsys.readouterr().out)
    assert len(marks) == 669
    assert set(marks[0].keys()) == {
        "group",
        "name",
        "message",
        "time",
        "duration",
        "end_time",
    }


def test_dump_counters_json(capsys):
    args = argparse.Namespace(
        capture_file=SAMPLE_CAPTURE_FILE, marks=False, counters=True, format="json"
    )
    dump.dump(args)

    counter_values = json.loads(capsys.readouterr().out)
    assert len(counter_values) == 540
    assert set(counter_values[0].keys()) == {
        "category",
        "name",
        "description",
        "time",
        "offset",
        "value",
    }


def test_analyze_text(capsys):
    args = argparse.Namespace(
        capture_file=SAMPLE_CAPTURE_FILE, format="text", timespan="-"
    )
    analyze.analyze(args)

    stdout = capsys.readouterr().out
    assert "Timespan: 0.0000 - 4.4673 [s]" in stdout
    assert "vblanks: 35" in stdout


def test_analyze_json(capsys):
    args = argparse.Namespace(
        capture_file=SAMPLE_CAPTURE_FILE, format="json", timespan="-"
    )
    analyze.analyze(args)

    stdout = capsys.readouterr().out
    report = json.loads(stdout)
    assert int(report["document"]["timespan"]["begin"]) == 0
    assert int(report["document"]["timespan"]["end"]) == 4467
    assert report["rendering"]["vblanks"] == 35


def test_analyze_json_statistics_cover_all_relevant_marks(capsys):
    args = argparse.Namespace(
        capture_file=SAMPLE_CAPTURE_FILE, format="json", timespan="-"
    )
    analyze.analyze(args)

    statistics = json.loads(capsys.readouterr().out)["statistics"]
    assert set(statistics) == set(analyze.MARKS_RELEVANT_FOR_STATISTICS)
    assert statistics["CompositingUpdate"]["duration"]["n"] == 7
    assert statistics["RenderTreeBuild"]["duration"]["n"] == 3
    # Tile geometry depends on the rendering backend, so only check that the
    # dirty area was extracted from every PaintTile message.
    assert statistics["PaintTile"]["dirty_pixels"]["n"] == 80
    assert statistics["PaintTile"]["dirty_pixels"]["min"] > 0


def test_analyze_with_custom_timespan(capsys):
    args = argparse.Namespace(
        capture_file=SAMPLE_CAPTURE_FILE, format="text", timespan="0-0"
    )
    analyze.analyze(args)

    stdout = capsys.readouterr().out
    assert "Timespan: 0.0000 - 0.0000 [s]" in stdout
    assert "vblanks: 0" in stdout


def test_analyze_with_custom_timespan_begin(capsys):
    args = argparse.Namespace(
        capture_file=SAMPLE_CAPTURE_FILE, format="text", timespan="500-"
    )
    analyze.analyze(args)

    stdout = capsys.readouterr().out
    assert "Timespan: 0.5000 - 4.4673 [s]" in stdout
    assert "vblanks: 25" in stdout


def test_analyze_with_custom_timespan_end(capsys):
    args = argparse.Namespace(
        capture_file=SAMPLE_CAPTURE_FILE, format="text", timespan="-500"
    )
    analyze.analyze(args)

    stdout = capsys.readouterr().out
    assert "Timespan: 0.0000 - 0.5000 [s]" in stdout
    assert "vblanks: 10" in stdout
