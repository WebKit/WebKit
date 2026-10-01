import argparse
import json
from pathlib import Path

import unittest

import tempfile

from .helpers import (
    SysprofTestCase,
    approx,
    build_minimal_elf,
    mark,
    stacktrace,
    stacktrace_data,
    sysprof_data,
)

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
            analyze._prepare_rendering_report(data, display_refreshes(data))[
                "vblanks_per_rendering_update"
            ],
            in_parse_order["vblanks_per_rendering_update"],
        )
        self.assertEqual(
            analyze._prepare_rendering_report(data, display_refreshes(data))[
                "vblank_interval_statistics"
            ],
            in_parse_order["vblank_interval_statistics"],
        )

    def test_a_capture_of_no_length_reports_no_rate(self):
        # A window of no length is rejected, but a capture of no length is the
        # capture's own doing and still has to be reported on.
        data = sysprof_data([mark("DidRenderFrame", 0, 0)], begin_msec=0, end_msec=0)

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

    def test_cycle_analysis_draws_a_bar_per_cycle_through_the_command_line(self):
        main(["cycle-analysis", "--color", "never", SAMPLE_CAPTURE_FILE])

        stdout = self.stdout()
        self.assertIn("Frame cycles: 2 in the analyzed window, 2 shown", stdout)
        # Two lanes per cycle, and the legend that says what the glyphs mean.
        self.assertEqual(stdout.count(" main \u2595"), 2)
        self.assertEqual(stdout.count(" tiles \u2595"), 2)
        self.assertIn("style resolution", stdout)

    def test_cycle_analysis_says_so_where_there_is_nothing_to_draw(self):
        main(["cycle-analysis", "-t", "0-370", SAMPLE_CAPTURE_FILE])
        # The capture holds the updates, the window cut through them, and saying it
        # holds none would send the reader looking for marks that are there.
        stdout = self.stdout()
        self.assertIn("No frame cycles found in the selected timespan", stdout)
        self.assertIn("widen --timespan", stdout)

    def test_cycle_analysis_says_so_where_the_selection_matches_nothing(self):
        main(["cycle-analysis", "--min-duration", "10000", SAMPLE_CAPTURE_FILE])
        self.assertIn("No frame cycles match the selection", self.stdout())

    def test_cycle_analysis_rejects_a_timespan_it_cannot_honour(self):
        for timespan in ["0-0", "5000-6000", "abc"]:
            with self.subTest(timespan=timespan), self.assertRaises(SystemExit):
                main(["cycle-analysis", "-t", timespan, SAMPLE_CAPTURE_FILE])

    def test_cycle_analysis_rejects_options_that_draw_nothing(self):
        # A usage error like every other one the tool reports, rather than a
        # traceback or a bar of one nonsense cell.
        for option in [
            ["--resolution", "0"],
            ["--resolution", "-1"],
            # Accepted by float(), and every comparison with one of them is False,
            # so they reach the drawing and raise there unless rejected here.
            ["--resolution", "nan"],
            ["--resolution", "inf"],
            ["--min-length", "nan"],
            ["--min-duration", "nan"],
            # Shorter than the nanosecond a capture is timed in, which rounds the
            # length of a cell to zero and draws every bar as idle.
            ["--resolution", "0.0000004"],
            ["--max-cycles", "0"],
            ["--max-cycles", "-1"],
            ["--max-cells", "0"],
            ["--min-duration", "-1"],
            ["--min-length", "-1"],
        ]:
            with self.subTest(option=option), self.assertRaises(SystemExit):
                main(["cycle-analysis"] + option + [SAMPLE_CAPTURE_FILE])

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

    def test_collapsed_stacktraces_resolve_addresses_against_the_process_maps(self):
        # Leaf (innermost, first in the raw stack) resolves against a known map; the
        # root (outermost, last in the raw stack), covered by no map and no bundled
        # symbol, resolves to nothing and is dropped, the same as Sysprof itself
        # drops a frame it cannot say anything at all about.
        data = stacktrace_data(
            [stacktrace(100, 100, [0x1050, 0x9999])],
            maps={
                100: [
                    {
                        "start": 0x1000,
                        "end": 0x2000,
                        "offset": 0,
                        "filename": "/usr/lib/libfoo.so",
                    }
                ]
            },
            processes={100: "/usr/bin/wpe-bare-app --headless"},
        )

        self.assertEqual(
            dump._collapsed_stacktraces_to_rows(data),
            [{"stack": "wpe-bare-app;In File /usr/lib/libfoo.so+0x50", "count": 1}],
        )

    def test_collapsed_stacktraces_drop_a_frame_covered_by_nothing_at_all(self):
        # No map, no bundled symbol: Sysprof's own address-layout lookup finds
        # nothing to even name the file, so the address is dropped rather than
        # shown as a bare address a reader could not recognize or search for.
        data = stacktrace_data([stacktrace(1, 1, [0x10])], processes={1: "app"})

        self.assertEqual(
            dump._collapsed_stacktraces_to_rows(data), [{"stack": "app", "count": 1}]
        )

    def test_collapsed_stacktraces_drop_the_earlier_of_two_overlapping_maps(self):
        # Reproduces exactly what sample.syscap's own ld-linux-aarch64.so.1 mapping
        # does to every address only it covers: it overlaps a second, later-starting
        # mapping ([vdso] there), and Sysprof's own SysprofAddressLayout drops the
        # earlier-starting of the two (see find_duplicates() in
        # sysprof-address-layout.c) rather than keep both, so an address inside the
        # earlier one but outside the later one resolves to nothing.
        data = stacktrace_data(
            [stacktrace(1, 1, [0x1050])],
            maps={
                1: [
                    # Starts first and is the wider of the two: dropped.
                    {
                        "start": 0x1000,
                        "end": 0x2000,
                        "offset": 0,
                        "filename": "/lib/loader.so",
                    },
                    # Starts second, inside the first one's range: kept.
                    {"start": 0x1800, "end": 0x1900, "offset": 0, "filename": "[vdso]"},
                ]
            },
            processes={1: "app"},
        )

        # 0x1050 is covered only by the dropped mapping, so nothing resolves it.
        self.assertEqual(
            dump._collapsed_stacktraces_to_rows(data), [{"stack": "app", "count": 1}]
        )

    def test_collapsed_stacktraces_offset_by_the_maps_own_file_offset(self):
        # A file mapped at a non-zero offset (e.g. a later segment of a shared
        # library) must have that offset folded back in, or the resolved address
        # would point elsewhere in the file than where the sample actually was.
        data = stacktrace_data(
            [stacktrace(1, 1, [0x2100])],
            maps={
                1: [
                    {
                        "start": 0x2000,
                        "end": 0x3000,
                        "offset": 0x500,
                        "filename": "/lib/x.so",
                    }
                ]
            },
            processes={1: "app"},
        )

        self.assertEqual(
            dump._collapsed_stacktraces_to_rows(data),
            [{"stack": "app;In File /lib/x.so+0x600", "count": 1}],
        )

    def test_collapsed_stacktraces_resolve_a_symbol_when_the_mapped_file_exists(self):
        # Unlike the two tests above, the mapped file here is a real one on disk, so
        # this exercises the whole path down into webkitsysprof.symbolizer rather
        # than just the "In File" fallback dump falls back to without it.
        with tempfile.TemporaryDirectory() as tempdir:
            library = str(Path(tempdir) / "libfoo.so")
            Path(library).write_bytes(build_minimal_elf([("my_function", 0x100, 0x50)]))

            data = stacktrace_data(
                [stacktrace(1, 1, [0x2110])],
                maps={
                    1: [
                        {
                            "start": 0x2000,
                            "end": 0x3000,
                            "offset": 0,
                            "filename": library,
                        }
                    ]
                },
                processes={1: "app"},
            )

            self.assertEqual(
                dump._collapsed_stacktraces_to_rows(data),
                [{"stack": "app;my_function+0x10", "count": 1}],
            )

    def test_collapsed_stacktraces_resolve_via_the_capture_s_own_bundled_symbols(self):
        # The capture's own "__symbols__" bundle (parsed by webkitsysprof.parser,
        # see parser_unittest.py) needs no map and no local file at all: sysprof
        # resolved this at record time, against whatever it saw mapped then.
        data = stacktrace_data(
            [stacktrace(100, 100, [0x1020])],
            processes={100: "app"},
            symbols={100: [(0x1000, 0x1050, "WebCore::TextureMapperLayer::paint")]},
        )

        self.assertEqual(
            dump._collapsed_stacktraces_to_rows(data),
            [{"stack": "app;WebCore::TextureMapperLayer::paint", "count": 1}],
        )

    def test_collapsed_stacktraces_prefer_bundled_symbols_over_the_map_fallback(self):
        # An address the bundle covers is resolved from it even where the sample's
        # maps would otherwise resolve to a real local ELF file, since the bundle is
        # what sysprof itself trusted at record time.
        with tempfile.TemporaryDirectory() as tempdir:
            library = str(Path(tempdir) / "libfoo.so")
            Path(library).write_bytes(
                build_minimal_elf([("wrong_function", 0x100, 0x50)])
            )

            data = stacktrace_data(
                [stacktrace(1, 1, [0x2110])],
                maps={
                    1: [
                        {
                            "start": 0x2000,
                            "end": 0x3000,
                            "offset": 0,
                            "filename": library,
                        }
                    ]
                },
                processes={1: "app"},
                symbols={1: [(0x2110, 0x2111, "right_function")]},
            )

            self.assertEqual(
                dump._collapsed_stacktraces_to_rows(data),
                [{"stack": "app;right_function", "count": 1}],
            )

    def test_collapsed_stacktraces_resolve_kernel_context_frames_via_kallsyms(self):
        # 0xffffffffffffff80 is PERF_CONTEXT_KERNEL: everything after it in the raw
        # (innermost-first) stack, until it ends or another marker appears, is a
        # kernel address and is resolved against the capture's bundled
        # /proc/kallsyms rather than the pid's own userspace maps/symbols, which a
        # kernel address would never fall inside. The stack never returns to user
        # space (no trailing marker), so it also gets the "- - Kernel - -" boundary
        # a stack that made it back out would get from a later marker instead (see
        # test_collapsed_stacktraces_turn_context_switch_markers_into_pseudo_frames).
        data = stacktrace_data(
            [stacktrace(1, 1, [0xFFFFFFFFFFFFFF80, 0xFFFF8000805E7240])],
            processes={1: "app"},
            kernel_symbols=[
                (0xFFFF8000805E7000, "drm_ioctl"),
                (0xFFFF8000805E7300, "next_fn"),
            ],
        )

        self.assertEqual(
            dump._collapsed_stacktraces_to_rows(data),
            [{"stack": "app;- - Kernel - -;drm_ioctl", "count": 1}],
        )

    def test_collapsed_stacktraces_fall_back_for_a_kernel_address_kallsyms_lacks(self):
        data = stacktrace_data(
            [stacktrace(1, 1, [0xFFFFFFFFFFFFFF80, 0x10])],
            processes={1: "app"},
            kernel_symbols=[(0x1000, "some_other_function")],
        )

        self.assertEqual(
            dump._collapsed_stacktraces_to_rows(data),
            [{"stack": "app;- - Kernel - -;In Kernel+0x10", "count": 1}],
        )

    def test_collapsed_stacktraces_add_a_kernel_boundary_when_the_unwind_never_returns(
        self,
    ):
        # A stack that enters the kernel and never gets back out (very common: a
        # kernel unwind frequently cannot walk back into whatever userspace code was
        # interrupted) reproduces the exact shape Sysprof's own callgraph shows for
        # this case: AllProcesses -> WPEWebProcess -> "Kernel Context Switch" ->
        # el0t_64_sync -> ... with nothing in between the process and the boundary,
        # since there is no userspace frame at all to put there.
        data = stacktrace_data(
            [stacktrace(2501, 2501, [0xFFFFFFFFFFFFFF80, 0x10, 0x20])],
            processes={2501: "WPEWebProcess"},
            kernel_symbols=[(0x10, "innermost_kernel_fn"), (0x20, "el0t_64_sync")],
        )

        self.assertEqual(
            dump._collapsed_stacktraces_to_rows(data),
            [
                {
                    "stack": "WPEWebProcess;- - Kernel - -;el0t_64_sync;innermost_kernel_fn",
                    "count": 1,
                }
            ],
        )

    def test_collapsed_stacktraces_turn_context_switch_markers_into_pseudo_frames(self):
        # 0xffffffffffffff80 is PERF_CONTEXT_KERNEL and 0xfffffffffffffe00 is
        # PERF_CONTEXT_USER, markers sysprof splices into the stack to say what
        # follows is in the kernel (or back in user space), not real addresses.
        # Sysprof's own callgraph turns a marker into a "- - <Context> - -"
        # pseudo-frame naming the context it *leaves* (so the one between the
        # kernel and user portions here reads "- - Kernel - -", not "- - User - -"),
        # except for a marker right at the start of the stack, which is dropped
        # since nothing preceded it to leave. What falls between two markers is
        # resolved as whatever context they say it is in: 0x30, in kernel context
        # here, is routed to the (empty, in this test) kallsyms table and falls
        # back to "In Kernel+0x30" rather than the plain "0x30" a userspace address
        # with nothing to resolve it against would show.
        data = stacktrace_data(
            [
                stacktrace(
                    1,
                    1,
                    [
                        0xFFFFFFFFFFFFFF80,  # Dropped: the very first address.
                        0x30,
                        0xFFFFFFFFFFFFFE00,  # "- - Kernel - -": leaves kernel context.
                    ],
                )
            ],
            processes={1: "app"},
        )

        self.assertEqual(
            dump._collapsed_stacktraces_to_rows(data),
            [{"stack": "app;- - Kernel - -;In Kernel+0x30", "count": 1}],
        )

    def test_collapsed_stacktraces_name_a_mid_stack_marker_by_the_context_it_leaves(
        self,
    ):
        # Reproduces the shape sysprof's own GUI shows for this exact capture's own
        # syscall stacks: "- - Kernel - -" marking the drop into the kernel,
        # directly under the process, with nothing in between, because the
        # unwind's one userspace address is covered by no map or bundled symbol at
        # all and is dropped, the same as Sysprof's own callgraph shows AllProcesses
        # -> WPEWebProcess -> "Kernel Context Switch" -> el0t_64_sync -> ... with no
        # frame of its own between the process and the boundary.
        data = stacktrace_data(
            [
                stacktrace(
                    1,
                    1,
                    [
                        0xFFFFFFFFFFFFFF80,  # Dropped: the very first address.
                        0x10,  # Innermost kernel frame.
                        0xFFFFFFFFFFFFFE00,  # "- - Kernel - -": leaves kernel context.
                        0x20,  # Outermost user frame, resolved by nothing, dropped.
                    ],
                )
            ],
            processes={1: "app"},
            kernel_symbols=[(0x10, "some_driver_function")],
        )

        self.assertEqual(
            dump._collapsed_stacktraces_to_rows(data),
            [{"stack": "app;- - Kernel - -;some_driver_function", "count": 1}],
        )

    def test_collapsed_stacktraces_name_the_process_by_pid_when_it_is_unknown(self):
        data = stacktrace_data(
            [stacktrace(42, 42, [0x10])], symbols={42: [(0x10, 0x11, "my_function")]}
        )

        self.assertEqual(
            dump._collapsed_stacktraces_to_rows(data),
            [{"stack": "pid 42;my_function", "count": 1}],
        )

    def test_collapsed_stacktraces_merge_identical_stacks_into_one_counted_row(self):
        data = stacktrace_data(
            [
                stacktrace(1, 1, [0x10]),
                stacktrace(1, 1, [0x10]),
                stacktrace(1, 1, [0x20]),
            ],
            processes={1: "app"},
            symbols={1: [(0x10, 0x11, "func_a"), (0x20, 0x21, "func_b")]},
        )

        # Sorted by stack, like stackcollapse-perf.pl's own output.
        self.assertEqual(
            dump._collapsed_stacktraces_to_rows(data),
            [
                {"stack": "app;func_a", "count": 2},
                {"stack": "app;func_b", "count": 1},
            ],
        )

    def test_collapsed_stacktraces_collapse_consecutive_identical_frames(self):
        # sysprof_document_symbolize_traceable() only counts a resolved symbol if it
        # differs from the one right before it, so several raw addresses landing in
        # a row inside the same bundled range (recursion, or a few samples of one
        # tight loop) must read as that one frame, not the same name repeated.
        data = stacktrace_data(
            [stacktrace(1, 1, [0x10, 0x18, 0x20])],
            processes={1: "app"},
            symbols={1: [(0x10, 0x28, "recursive_fn")]},
        )

        self.assertEqual(
            dump._collapsed_stacktraces_to_rows(data),
            [{"stack": "app;recursive_fn", "count": 1}],
        )

    def test_collapsed_stacktraces_do_not_collapse_different_symbols_with_one_name(
        self,
    ):
        # Two distinct symbols that merely demangle to an identical name (e.g. a
        # class's two constructor overloads) are not the same frame, so collapsing
        # must compare by resolved range/identity, never by the display string.
        data = stacktrace_data(
            [stacktrace(1, 1, [0x10, 0x20])],
            processes={1: "app"},
            symbols={
                1: [
                    (0x10, 0x18, "SomeClass::SomeClass"),
                    (0x20, 0x28, "SomeClass::SomeClass"),
                ]
            },
        )

        self.assertEqual(
            dump._collapsed_stacktraces_to_rows(data),
            [{"stack": "app;SomeClass::SomeClass;SomeClass::SomeClass", "count": 1}],
        )

    def test_collapsed_stacktraces_show_unwindable_for_a_lone_context_switch(self):
        # sysprof_callgraph_add_traceable()'s "corrupted unwind" case: a traceable
        # whose only content is a single context-switch marker (so nothing real was
        # captured at all) is shown as a distinct "Unwindable" frame instead of an
        # empty stack, so it reads as a broken/empty recording rather than as if the
        # process simply had no stack of its own.
        data = stacktrace_data(
            [stacktrace(1, 1, [0xFFFFFFFFFFFFFE00])],
            processes={1: "app"},  # PERF_CONTEXT_USER
        )

        self.assertEqual(
            dump._collapsed_stacktraces_to_rows(data),
            [{"stack": "app;Unwindable", "count": 1}],
        )

    def test_collapsed_stacktraces_do_not_show_unwindable_for_a_lone_kernel_switch(
        self,
    ):
        # The "Unwindable" substitution is specific to sysprof_callgraph_add_traceable
        # checking `final_context == SYSPROF_ADDRESS_CONTEXT_USER`; a stack that
        # starts and stays in the kernel (nothing to unwind back out of) still gets
        # its ordinary "- - Kernel - -" boundary instead.
        data = stacktrace_data(
            [stacktrace(1, 1, [0xFFFFFFFFFFFFFF80])],
            processes={1: "app"},  # PERF_CONTEXT_KERNEL
        )

        self.assertEqual(
            dump._collapsed_stacktraces_to_rows(data),
            [{"stack": "app;- - Kernel - -", "count": 1}],
        )

    def test_process_name_keeps_a_kernel_thread_s_own_slash(self):
        # A kernel thread's "comm" can itself contain a "/" (e.g. numbered instances
        # of one worker pool), which is not a filesystem path: running it through
        # basename() would both mangle it and silently merge unrelated threads that
        # happen to share a trailing number ("migration/0" and "ksoftirqd/0" would
        # otherwise both become the process name "0").
        data = stacktrace_data(
            [stacktrace(16, 16, [0x10]), stacktrace(18, 18, [0x10])],
            processes={16: "migration/0", 18: "ksoftirqd/0"},
            symbols={16: [(0x10, 0x11, "fn")], 18: [(0x10, 0x11, "fn")]},
        )

        self.assertEqual(
            dump._collapsed_stacktraces_to_rows(data),
            [
                {"stack": "ksoftirqd/0;fn", "count": 1},
                {"stack": "migration/0;fn", "count": 1},
            ],
        )

    def test_process_name_still_strips_a_real_path_s_directory(self):
        data = stacktrace_data(
            [stacktrace(1, 1, [0x10])],
            processes={1: "/usr/bin/wpe-bare-app --headless"},
            symbols={1: [(0x10, 0x11, "fn")]},
        )

        self.assertEqual(
            dump._collapsed_stacktraces_to_rows(data),
            [{"stack": "wpe-bare-app;fn", "count": 1}],
        )

    def test_dump_collapsed_stacktraces_through_the_command_line(self):
        main(["dump", "--collapsed-stacktraces", SAMPLE_CAPTURE_FILE])

        stdout = self.stdout()
        lines = stdout.splitlines()
        stacks = [line.rsplit(" ", 1)[0] for line in lines]
        counts = [int(line.rsplit(" ", 1)[1]) for line in lines]

        self.assertEqual(len(lines), 720)
        # Every one of the capture's 1026 SAMPLE frames is accounted for exactly once.
        self.assertEqual(sum(counts), 1026)
        # Folded output is sorted, like stackcollapse-perf.pl's own.
        self.assertEqual(stacks, sorted(stacks))
        # The capture bundles its own record-time symbolization (see
        # webkitsysprof.parser's "__symbols__.gz" handling), so real, demangled
        # WebCore/WebKit function names come back without any local binary at all.
        self.assertIn("WebCore::TextureMapperLayer::paint", stdout)
        self.assertIn("In File /usr/lib/libWPEWebKit-2.0.so.1.10.0", stdout)
        # And the capture's bundled /proc/kallsyms.gz (see parser_unittest.py's
        # kallsyms tests) resolves kernel-context frames, e.g. an ioctl() into DRM,
        # to real kernel function names too.
        self.assertIn("drm_ioctl", stdout)
        # Matching Sysprof itself exactly (verified against sysprof-cat and
        # test-symbolize built from the actual sysprof sources): this capture's own
        # ld-linux-aarch64.so.1 mapping overlaps its neighboring [vdso] mapping, so
        # Sysprof's own address-layout dedup drops it, and every address only it
        # covered resolves to nothing and is dropped rather than shown as a bare
        # address; a raw hex address would mean that dedup was not applied.
        self.assertNotIn("ld-linux", stdout)
        self.assertNotIn(";0x", stdout)
        # A kernel thread's own "comm" keeps its "/" (this capture's kworker
        # threads would otherwise collide on their trailing number, or lose their
        # "kworker/" prefix entirely).
        self.assertIn("kworker/1:2-events", stdout)
        # A traceable whose only content is a single context-switch marker (no real
        # frame at all) reads as "Unwindable", matching sysprof-cat's own totals
        # for this capture exactly (WPENetworkProce 79, WPEWebProcess 89,
        # wpe-bare-app 69).
        self.assertIn("WPENetworkProce;Unwindable 79", stdout)
        self.assertIn("WPEWebProcess;Unwindable 89", stdout)
        self.assertIn("wpe-bare-app;Unwindable 69", stdout)
        # Consecutive raw addresses landing in the same bundled range collapse to
        # one frame instead of repeating it. (Two adjacent frames can still show
        # the same *text*, e.g. WebCore::GraphicsLayerCoordinated::
        # ~GraphicsLayerCoordinated() appears twice in a row further down this same
        # capture — verified against sysprof-cat as two genuinely different C1/C2
        # destructor symbols that merely demangle identically, so collapsing must
        # compare resolved identity, not display text; see the dedicated
        # do-not-collapse-same-name test above.)
        self.assertNotIn(
            "WebCore::GraphicsLayerCoordinated::updateBackingStoresIfNeededv;"
            "WebCore::GraphicsLayerCoordinated::updateBackingStoresIfNeededv",
            stdout,
        )
        # A bundled symbol's "nick" (which library it belongs to) is kept alongside
        # its name rather than dropped.
        self.assertIn("(GLib)", stdout)

    def test_dump_collapsed_stacktraces_json_through_the_command_line(self):
        main(["dump", "--collapsed-stacktraces", "-f", "json", SAMPLE_CAPTURE_FILE])

        rows = json.loads(self.stdout())
        self.assertEqual(len(rows), 720)
        self.assertEqual(sum(row["count"] for row in rows), 1026)
        self.assertEqual(set(rows[0].keys()), {"stack", "count"})
        self.assertTrue(any("WebCore::" in row["stack"] for row in rows))

    def test_collapsed_stacktraces_is_mutually_exclusive_with_marks_and_counters(self):
        with self.assertRaises(SystemExit):
            main(["dump", "--marks", "--collapsed-stacktraces", SAMPLE_CAPTURE_FILE])
        with self.assertRaises(SystemExit):
            main(["dump", "--counters", "--collapsed-stacktraces", SAMPLE_CAPTURE_FILE])

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
