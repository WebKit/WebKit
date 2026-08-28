import argparse
import json
from pathlib import Path

import unittest

from .helpers import SysprofTestCase, approx, mark, sysprof_data

from webkitsysprof import summary, dump, analyze, histogram
from webkitsysprof.__main__ import main
from webkitsysprof.utils import (
    UsageError,
    display_refreshes,
    check_timespan_holds_data,
    msec_to_nsec,
    trim_marks_by_name_to_timespan,
)

SAMPLE_CAPTURE_FILE = str(Path(__file__).parent / "assets" / "sample.syscap")


class SubcommandsTest(SysprofTestCase):
    def test_summary(self):
        args = argparse.Namespace(capture_file=SAMPLE_CAPTURE_FILE)
        summary.summary(args)

        stdout = self.stdout()
        self.assertIn(f"File: {SAMPLE_CAPTURE_FILE}", stdout)
        self.assertIn("Marks: 669", stdout)
        self.assertIn("Counters: 53", stdout)

    def test_dump_marks_csv(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, marks=True, counters=False, format="csv"
        )
        dump.dump(args)

        stdout_lines = self.stdout().split("\n")
        self.assertEqual(len(stdout_lines), 669 + 1 + 1)

    def test_dump_counters_csv(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, marks=False, counters=True, format="csv"
        )
        dump.dump(args)

        stdout_lines = self.stdout().split("\n")
        self.assertEqual(len(stdout_lines), 540 + 1 + 1)

    def test_dump_marks_json(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, marks=True, counters=False, format="json"
        )
        dump.dump(args)

        marks = json.loads(self.stdout())
        self.assertEqual(len(marks), 669)
        self.assertEqual(
            set(marks[0].keys()),
            {
                "group",
                "pid",
                "name",
                "message",
                "time",
                "duration",
                "end_time",
            },
        )

    def test_dump_counters_json(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, marks=False, counters=True, format="json"
        )
        dump.dump(args)

        counter_values = json.loads(self.stdout())
        self.assertEqual(len(counter_values), 540)
        self.assertEqual(
            set(counter_values[0].keys()),
            {
                "category",
                "name",
                "description",
                "time",
                "offset",
                "value",
            },
        )

    def test_analyze_text(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, format="text", timespan="-", explain=False
        )
        analyze.analyze(args)

        stdout = self.stdout()
        self.assertIn("Timespan: 0.0000 - 4.4673 [s]", stdout)
        self.assertIn("vblanks: 35", stdout)
        self.assertIn("Frame cycle:", stdout)
        self.assertIn("- cycles: 2", stdout)
        # The explanations are opt-in, so the report itself stays diffable.
        self.assertNotIn(
            "Theoretical FPS is the number of DidRenderFrame marks", stdout
        )
        self.assertNotIn(
            "A frame cycle starts when a LayerTreeHostRenderingUpdate", stdout
        )

    def test_analyze_text_with_explanations(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, format="text", timespan="-", explain=True
        )
        analyze.analyze(args)

        stdout = self.stdout()
        self.assertIn("Theoretical FPS is the number of DidRenderFrame marks", stdout)
        self.assertIn(
            "A frame cycle starts when a LayerTreeHostRenderingUpdate begins", stdout
        )
        self.assertIn("StyleRecalc is an umbrella mark", stdout)

    def test_analyze_json_percentiles_stay_within_the_data_range(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, format="json", timespan="-", explain=False
        )
        analyze.analyze(args)

        report = json.loads(self.stdout())
        all_statistics = (
            [
                statistics
                for mark in report["statistics"].values()
                for statistics in mark.values()
            ]
            + [
                report["frame_cycle"]["duration_statistics"],
                report["frame_cycle"]["resolved_duration_statistics"],
                # The ones the text report prints percentiles of inline.
                report["rendering"]["vblank_interval_statistics"],
                report["rendering"]["vblanks_per_rendering_update"]["statistics"],
                report["rendering"]["frame_compositions_per_vblank_statistics"],
            ]
            + list(report["frame_cycle"]["phase_statistics"].values())
        )

        # Extrapolating past the samples used to yield a P25 below the minimum, a P99
        # above the maximum and negative durations.
        for statistics in all_statistics:
            for percentile in statistics.get("percentiles", {}).values():
                self.assertTrue(statistics["min"] <= percentile <= statistics["max"])

    def test_analyze_json(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, format="json", timespan="-", explain=False
        )
        analyze.analyze(args)

        stdout = self.stdout()
        report = json.loads(stdout)
        self.assertEqual(int(report["document"]["timespan"]["begin"]), 0)
        self.assertEqual(int(report["document"]["timespan"]["end"]), 4467)
        self.assertEqual(report["rendering"]["vblanks"], 35)
        self.assertEqual(report["frame_cycle"]["cycles"], 2)
        self.assertEqual(report["frame_cycle"]["resolved_duration_statistics"]["n"], 2)
        self.assertEqual(
            set(report["frame_cycle"]["phase_statistics"]),
            {
                "rendering_update",
                "waiting_for_compositing",
                "compositing",
                "idle",
            },
        )

    def test_analyze_json_frame_cycle_phases_add_up_to_cycle_duration(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, format="json", timespan="-", explain=False
        )
        analyze.analyze(args)

        frame_cycle = json.loads(self.stdout())["frame_cycle"]
        phases_mean = sum(
            statistics["mean"]
            for statistics in frame_cycle["phase_statistics"].values()
        )
        # Against the cycles the phases came from, not all of them: the two differ as
        # soon as one cycle cannot be split into phases.
        self.assertEqual(
            phases_mean,
            approx(frame_cycle["resolved_duration_statistics"]["mean"], rel=1e-6),
        )

    def test_analyze_json_statistics_cover_all_relevant_marks(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, format="json", timespan="-", explain=False
        )
        analyze.analyze(args)

        statistics = json.loads(self.stdout())["statistics"]
        self.assertEqual(set(statistics), set(analyze.MARKS_RELEVANT_FOR_STATISTICS))
        self.assertEqual(statistics["CompositingUpdate"]["duration"]["n"], 7)
        self.assertEqual(statistics["RenderTreeBuild"]["duration"]["n"], 3)
        # Tile geometry depends on the rendering backend, so only check that the
        # dirty area was extracted from every PaintTile message.
        self.assertEqual(statistics["PaintTile"]["dirty_pixels"]["n"], 80)
        self.assertGreater(statistics["PaintTile"]["dirty_pixels"]["min"], 0)

    def test_analyze_with_a_timespan_holding_vblanks_but_no_rendering_update(self):
        # The window has vblanks, so the no-vblanks early return does not save it, and
        # trimming drops the first rendering update for reaching past the window end.
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE,
            format="text",
            timespan="0-370",
            explain=False,
        )
        analyze.analyze(args)

        stdout = self.stdout()
        self.assertIn("- cycles: 0", stdout)
        self.assertIn("no two consecutive LayerTreeHostRenderingUpdate marks", stdout)

    def test_analyze_json_leaves_out_a_cycle_reaching_past_the_timespan(self):
        # The only cycle of this window begins at 371.6 ms and ends at 524.2 ms, so it
        # is no frame of the window and must not be timed as one.
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE,
            format="json",
            timespan="0-400",
            explain=False,
        )
        analyze.analyze(args)

        frame_cycle = json.loads(self.stdout())["frame_cycle"]
        self.assertEqual(frame_cycle["cycles"], 0)
        self.assertEqual(frame_cycle["coverage"], approx(0.0))

    def test_analyze_resolves_a_cycle_whose_compositing_marks_trimming_drops(self):
        # The cycle 524.2 -> 560.8 ms lies within this window, but composites in
        # RenderLayerTree 549.8 -> 571.1, which reaches past the window end and is
        # trimmed away. Reconstructing from the untrimmed capture keeps it.
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE,
            format="json",
            timespan="370-561",
            explain=False,
        )
        analyze.analyze(args)

        frame_cycle = json.loads(self.stdout())["frame_cycle"]
        self.assertEqual(frame_cycle["cycles"], 2)
        self.assertEqual(frame_cycle["resolved_duration_statistics"]["n"], 2)
        # Without that mark the composition looks as if it ended with PaintToGLContext
        # at 560.6 ms, just inside its cycle, so the overrun would go unnoticed.
        self.assertEqual(frame_cycle["overrunning_composition_statistics"]["n"], 2)

    def test_explain_is_rejected_for_the_json_format_by_the_module_api(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, format="json", timespan="-", explain=True
        )
        with self.assertRaises(UsageError):
            analyze.analyze(args)

    def test_analyze_rejects_a_timespan_it_cannot_honour(self):
        # Silently analyzing the whole capture, or a window the argument never asked
        # for, reads as a result for the requested window.
        for timespan in [
            "abc",
            "0-1e3",
            "1-2-3",
            "5000-",
            "5000-6000",
            "",
            # Of no length, so it encloses nothing to analyze.
            "0-0",
            "500-500",
        ]:
            with self.subTest(timespan=timespan):
                args = argparse.Namespace(
                    capture_file=SAMPLE_CAPTURE_FILE,
                    format="text",
                    timespan=timespan,
                    explain=False,
                )
                with self.assertRaises(UsageError):
                    analyze.analyze(args)

                with self.assertRaises(SystemExit):
                    main(["analyze", "-t", timespan, SAMPLE_CAPTURE_FILE])

    def test_analyze_clamps_a_timespan_reaching_past_the_capture(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE,
            format="json",
            timespan="0-100000",
            explain=False,
        )
        analyze.analyze(args)

        report = json.loads(self.stdout())
        # The rates divide by the analyzed duration, so a window stretching past the
        # capture would water every one of them down.
        self.assertEqual(int(report["document"]["timespan"]["end"]), 4467)
        self.assertEqual(
            report["rendering"]["theoretical_fps"], approx(0.448, abs=0.001)
        )

    def test_vblanks_per_rendering_update_does_not_depend_on_the_mark_order(self):
        # The walk needs the updates in begin order, which the grouping does not grant.
        parsed = analyze.parse(SAMPLE_CAPTURE_FILE, marks=True, counters=False)
        data = analyze.sysprof_data_with_marks_by_name(parsed)
        in_parse_order = analyze._prepare_rendering_report(
            data, display_refreshes(data)
        )

        for marks in data["marks"].values():
            marks.reverse()
        self.assertEqual(
            analyze._prepare_rendering_report(
                data, display_refreshes(data)
            )["vblanks_per_rendering_update"],
            in_parse_order["vblanks_per_rendering_update"],
        )
        self.assertEqual(
            analyze._prepare_rendering_report(
                data, display_refreshes(data)
            )["vblank_interval_statistics"],
            in_parse_order["vblank_interval_statistics"],
        )

    def test_a_capture_of_no_length_reports_no_rate(self):
        # A window of no length is rejected, but a capture of no length is the
        # capture's own doing and still has to be reported on.
        data = sysprof_data(
            [mark("DidRenderFrame", 0, 0)], begin_msec=0, end_msec=0
        )

        report = analyze._prepare_report(data, data)

        frame_cycle = report["frame_cycle"]
        # No cycle and no duration to divide, rather than a cycle of zero length that
        # fits in zero vblank intervals and covers 0% of nothing.
        self.assertEqual(frame_cycle["cycles"], 0)
        self.assertIsNone(frame_cycle["implied_fps"])
        self.assertIsNone(frame_cycle["vblank_intervals_per_cycle"])
        self.assertIsNone(frame_cycle["coverage"])
        # No duration to divide frames by either.
        self.assertIsNone(report["rendering"]["theoretical_fps"])

    def test_delta_histogram_deltas_come_from_the_requested_mark(self):
        parsed = histogram.parse(SAMPLE_CAPTURE_FILE, marks=True, counters=False)
        data = histogram.sysprof_data_with_marks_by_name(parsed)

        deltas = histogram.intervals_between_marks(
            histogram.marks_in_time_order(data, "DisplayLinkUpdate")
        )
        self.assertEqual(len(deltas), 34)
        self.assertGreater(min(deltas), 0)
        self.assertEqual(
            histogram.intervals_between_marks(
                histogram.marks_in_time_order(data, "NoSuchMark")
            ),
            [],
        )
        self.assertTrue(10 <= histogram._calculate_optimal_bins(deltas) <= 100)

    def test_delta_histogram_honours_the_timespan(self):
        parsed = histogram.parse(SAMPLE_CAPTURE_FILE, marks=True, counters=False)
        data = histogram.trim_marks_by_name_to_timespan(
            histogram.sysprof_data_with_marks_by_name(parsed), None, msec_to_nsec(500)
        )

        self.assertEqual(len(data["marks"]["DisplayLinkUpdate"]), 10)
        self.assertEqual(
            len(
                histogram.intervals_between_marks(
                    histogram.marks_in_time_order(data, "DisplayLinkUpdate")
                )
            ),
            9,
        )

    def test_analyze_json_counts_every_vblank_interval_of_the_timespan(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, format="json", timespan="-", explain=False
        )
        analyze.analyze(args)

        rendering = json.loads(self.stdout())["rendering"]
        # 35 vblanks are 34 intervals, and the ones after the last composition are
        # samples too. Counting only up to it used to report 15 and a mean twice as
        # high as the capture actually composited.
        self.assertEqual(rendering["vblanks"], 35)
        self.assertEqual(rendering["frame_compositions_per_vblank_statistics"]["n"], 34)
        self.assertEqual(
            rendering["frame_compositions_per_vblank_statistics"]["mean"],
            approx(2 / 34),
        )

    def test_frame_compositions_outside_the_vblank_range_are_left_out(self):
        vblanks = [mark("DisplayLinkUpdate", msec, msec) for msec in (0, 16, 32)]
        compositions = [
            mark("DidRenderFrame", msec, msec) for msec in (5, 40, 45, 50, 55)
        ]

        # The four compositions after the last vblank belong to no interval between
        # two refreshes. Booking them into the last one made up a burst that the
        # capture never had.
        self.assertEqual(
            analyze._calculate_frame_compositions_per_vblank(vblanks, compositions),
            [
                1,
                0,
            ],
        )

    def test_frame_rendering_reasons_bucket_a_frame_that_named_none(self):
        def did_render_frame(message, msec=0):
            return {
                "name": "DidRenderFrame",
                "message": message,
                "duration": 0,
                "end_time": msec_to_nsec(msec),
            }

        data = {
            "document": {"timespan": {"begin": 0, "end": msec_to_nsec(1000)}},
            "marks": {
                "DidRenderFrame": [
                    did_render_frame("reasons: Scrolling"),
                    did_render_frame("reasons: Scrolling, AsyncScrolling", 1),
                    did_render_frame("reasons: ", 2),
                    did_render_frame("reasons: ", 3),
                ]
            },
        }

        # The reasons of a frame are one bucket, however many it names, and the
        # frames that named none share one of their own.
        self.assertEqual(
            analyze._prepare_rendering_report(data, display_refreshes(data))[
                "frame_rendering_reasons"
            ],
            {"Scrolling": 1, "Scrolling, AsyncScrolling": 1, "_none": 2},
        )

    def test_explain_reaches_the_report_through_the_command_line(self):
        # Through main(), so that renaming the flag or its dest cannot quietly stop the
        # explanations from being printed.
        main(["analyze", "-e", SAMPLE_CAPTURE_FILE])

        stdout = self.stdout()
        self.assertIn(
            "A frame cycle starts when a LayerTreeHostRenderingUpdate begins", stdout
        )
        self.assertIn("StyleRecalc is an umbrella mark", stdout)

    def test_a_capture_of_its_own_broken_timespan_is_no_usage_error(self):
        # No -t was passed, so nothing the user typed can be at fault.
        data = {"document": {"timespan": {"begin": 0, "end": -5}}, "marks": {}}
        self.assertEqual(
            trim_marks_by_name_to_timespan(data, None, None)["document"]["timespan"][
                "end"
            ],
            -5,
        )

    def test_statistics_are_matched_by_wording_rather_than_word_position(self):
        extract = analyze.STATISTICAL_DATA_EXTRACTORS["UpdateTiles"]

        self.assertEqual(
            extract({"message": "dirty tiles: 40", "duration": 0})["tiles"], 40
        )
        # A mark that carries no message at all leaves the statistic out, rather
        # than reporting a number read from somewhere else.
        self.assertIsNone(extract({"message": "", "duration": 0})["tiles"])

    def test_explaining_an_empty_frame_cycle_section_still_explains_it(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE,
            format="text",
            timespan="0-370",
            explain=True,
        )
        analyze.analyze(args)

        stdout = self.stdout()
        self.assertIn("- cycles: 0", stdout)
        # An empty section is what raises the question the explanation answers.
        self.assertIn(
            "A frame cycle starts when a LayerTreeHostRenderingUpdate begins", stdout
        )

    def test_a_broken_capture_is_no_usage_error_even_with_a_timespan(self):
        # -t 0- asks for exactly the capture's own range, so nothing the user typed
        # can be at fault when that range runs backwards or is of no length.
        check_timespan_holds_data(0, msec_to_nsec(-5), 0, None)
        check_timespan_holds_data(0, msec_to_nsec(-5), None, None)
        check_timespan_holds_data(0, 0, 0, msec_to_nsec(5))

    def test_a_window_meeting_the_capture_at_one_point_is_rejected(self):
        capture = (msec_to_nsec(100), msec_to_nsec(200))
        # Clamped to the capture, either of these encloses a single instant, which
        # is no duration to divide by and no range for a mark to fall inside.
        with self.assertRaises(UsageError):
            check_timespan_holds_data(*capture, msec_to_nsec(200), None)
        with self.assertRaises(UsageError):
            check_timespan_holds_data(*capture, None, msec_to_nsec(100))
        # One millisecond of overlap is still a window.
        check_timespan_holds_data(*capture, msec_to_nsec(199), None)
        check_timespan_holds_data(*capture, None, msec_to_nsec(101))

    def test_json_percentiles_keep_the_fiftieth(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, format="json", timespan="-", explain=False
        )
        analyze.analyze(args)

        percentiles = json.loads(self.stdout())["statistics"]["StyleRecalc"][
            "duration"
        ]["percentiles"]
        # Read by consumers since before the frame cycle section existed.
        self.assertEqual(sorted(percentiles), ["25", "50", "75", "99"])

    def test_delta_histogram_keeps_the_processes_apart(self):
        # Two processes rendering at 16 ms, offset by 8 ms from one another.
        marks = []
        for i in range(5):
            marks.append(
                mark("LayerTreeHostRenderingUpdate", i * 16, i * 16 + 2, pid=1)
            )
            marks.append(
                mark("LayerTreeHostRenderingUpdate", i * 16 + 8, i * 16 + 10, pid=2)
            )
        data = sysprof_data(marks, end_msec=100)

        # Merged, the deltas would read 8 ms and the histogram would peak at half the
        # frame interval of either process.
        deltas = histogram._delta_times_ms(data, "LayerTreeHostRenderingUpdate")
        self.assertEqual(deltas, [approx(16.0)] * 8)

    def test_composition_overrun_says_nothing_where_nothing_was_analyzed(self):
        frame_cycle = {
            "cycles": 9,
            "processes": 1,
            "duration_statistics": {"n": 9, "min": 1, "max": 1, "median": 1, "mean": 1},
            "resolved_duration_statistics": {},
            "phase_statistics": {phase: {} for phase in analyze.PHASES},
            "overrunning_composition_statistics": {},
            "implied_fps": 1.0,
            "coverage": 0.5,
            "capture_vblank_interval": 16.0,
            "vblank_intervals_per_cycle": 1.0,
            "vblank_intervals_per_cycle_unknown": None,
        }

        analyze._render_frame_cycle_numbers(frame_cycle)

        stdout = self.stdout()
        # "in none of 0 analyzed cycles" would read as a measurement never made.
        self.assertIn("- composition overrunning the cycle: -", stdout)
        # The note explains shares, and every phase here reads as -.
        self.assertNotIn("need not total 100%", stdout)

    def test_an_update_ending_on_the_first_refresh_spans_it(self):
        vblanks = [mark("DisplayLinkUpdate", msec, msec) for msec in (100, 116, 132)]
        update = [mark("LayerTreeHostRenderingUpdate", 90, 100)]

        # It ran up to that refresh, so it spanned one, and both ends of the range
        # are read the same way.
        self.assertEqual(
            analyze._calculate_vblanks_per_rendering_update(vblanks, update), [1]
        )

    def test_refreshes_that_composited_nothing_are_samples_of_nothing(self):
        vblanks = [
            mark("DisplayLinkUpdate", i * 16, i * 16, "WebKit (UI)", pid=1)
            for i in range(60)
        ]
        data = sysprof_data(vblanks, end_msec=1000)

        rendering = analyze._prepare_rendering_report(data, vblanks)

        # A capture that composited nothing is not a capture without refreshes: every
        # interval held no composition, which is 59 samples of zero.
        statistics = rendering["frame_compositions_per_vblank_statistics"]
        self.assertEqual(statistics["n"], 59)
        self.assertEqual(statistics["max"], 0)

    def test_dump_csv_carries_every_column_of_a_row(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, marks=True, counters=False, format="csv"
        )
        dump.dump(args)

        header = self.stdout().splitlines()[0]
        self.assertEqual(header, "group;pid;name;message;time;duration;end_time")

    def test_a_window_without_refreshes_keeps_the_capture_interval(self):
        # The link stops before the window begins. Its interval is a property of the
        # display, so the cycles of the window are still measured in it.
        marks = [
            mark("DisplayLinkUpdate", i * 16, i * 16, "WebKit (UI)", pid=1)
            for i in range(30)
        ]
        marks += [
            mark("LayerTreeHostRenderingUpdate", 600 + i * 20, 600 + i * 20 + 5, pid=2)
            for i in range(10)
        ]
        parsed = {"document": {"timespan": [0, msec_to_nsec(1000)]}, "marks": marks}
        untrimmed = analyze.sysprof_data_with_marks_by_name(parsed)
        window = analyze.trim_marks_by_name_to_timespan(
            untrimmed, msec_to_nsec(600), msec_to_nsec(800)
        )

        report = analyze._prepare_report(window, untrimmed)

        self.assertEqual(report["rendering"]["vblanks"], 0)
        self.assertEqual(report["frame_cycle"]["capture_vblank_interval"], approx(16.0))
        self.assertEqual(
            report["frame_cycle"]["vblank_intervals_per_cycle"], approx(20 / 16)
        )

    def test_analyze_with_custom_timespan(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE,
            format="text",
            timespan="0-370",
            explain=False,
        )
        analyze.analyze(args)

        stdout = self.stdout()
        self.assertIn("Timespan: 0.0000 - 0.3700 [s]", stdout)
        self.assertIn("vblanks: 2", stdout)

    def test_analyze_with_custom_timespan_begin(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE,
            format="text",
            timespan="500-",
            explain=False,
        )
        analyze.analyze(args)

        stdout = self.stdout()
        self.assertIn("Timespan: 0.5000 - 4.4673 [s]", stdout)
        self.assertIn("vblanks: 25", stdout)

    def test_analyze_with_custom_timespan_end(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE,
            format="text",
            timespan="-500",
            explain=False,
        )
        analyze.analyze(args)

        stdout = self.stdout()
        self.assertIn("Timespan: 0.0000 - 0.5000 [s]", stdout)
        self.assertIn("vblanks: 10", stdout)


if __name__ == "__main__":
    unittest.main()
