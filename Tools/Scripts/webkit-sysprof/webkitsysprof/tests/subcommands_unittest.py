import argparse
import json
import tempfile
import unittest
from pathlib import Path

from .helpers import (
    SysprofTestCase,
    approx,
    build_minimal_elf,
    composition,
    frame_complete,
    presented,
    mark,
    rendering_update,
    requested,
    scheduled,
    stacktrace,
    stacktrace_data,
    steady_animation,
    sysprof_data,
)

from webkitsysprof import summary, dump, analyze, histogram
from webkitsysprof.__main__ import main
from webkitsysprof.utils import (
    UsageError,
    check_timespan_holds_data,
    msec_to_nsec,
    trim_marks_by_name_to_timespan,
)

from webkitsysprof.updates import UPDATE_PHASES

SAMPLE_CAPTURE_FILE = str(Path(__file__).parent / "assets" / "sample.syscap")
RU, ANIMATION = "RenderingUpdate", "Animation"


def asking_for_a_frame(begin, end, request_at):
    """A rendering update that asks for a composition while the compositor idles."""
    return [
        rendering_update(begin, end),
        scheduled(request_at, "Idle", reasons=(RU,)),
        requested(request_at + 0.01),
    ]


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
        self.assertIn("Compositions:", stdout)
        self.assertIn("- compositions: 2", stdout)
        self.assertIn("| update-driven | RenderingUpdate | 2 | 100.00% |", stdout)
        self.assertIn("Rendering updates:", stdout)
        self.assertIn("- updates: 3, asked for a composition: 2, composited: 2", stdout)
        # Explanations are opt-in, keeping the report diffable.
        self.assertNotIn("A composition is a RenderLayerTree", stdout)
        self.assertNotIn("A rendering update is a", stdout)

    def test_analyze_text_with_explanations(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, format="text", timespan="-", explain=True
        )
        analyze.analyze(args)

        stdout = self.stdout()
        self.assertIn(
            "Frame intervals are measured against the refresh interval", stdout
        )
        self.assertIn("A composition is a RenderLayerTree", stdout)
        self.assertIn("- active frame interval: a frame interval in which", stdout)
        self.assertIn("- update-to-frame latency: from update begin", stdout)
        self.assertIn("Compositor time replays", stdout)
        self.assertIn("A rendering update is a LayerTreeHostRenderingUpdate", stdout)
        self.assertIn("StyleRecalc wraps RenderTreeBuild", stdout)

    def test_analyze_json_explanations_mirror_the_report(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, format="json", timespan="-", explain=False
        )
        analyze.analyze(args)
        report = json.loads(self.stdout())
        args.explain = True
        analyze.analyze(args)
        start = len(json.dumps(report)) + 1
        explained = json.loads(self.stdout()[start:])

        def is_statistics(value):
            return isinstance(value, dict) and {"n", "median"} <= set(value)

        def check(value, explanation, path):
            # A string explains a whole subtree, such as a statistics dict.
            if isinstance(explanation, str):
                return
            if isinstance(value, list):
                for item in value:
                    check(item, explanation[0], path + "[]")
                return
            self.assertIsInstance(value, dict, path)
            if set(explanation) == {"<pid>"}:
                for pid, item in value.items():
                    check(item, explanation["<pid>"], path + "." + pid)
                return
            # A mark the capture lacks has no statistics, so allow fewer keys.
            self.assertLessEqual(set(value), set(explanation), path)
            for key, item in value.items():
                if not is_statistics(item):
                    check(item, explanation[key], path + "." + key)

        check(report, explained, "report")

    def test_analyze_json_percentiles_stay_within_the_data_range(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, format="json", timespan="-", explain=False
        )
        analyze.analyze(args)

        report = json.loads(self.stdout())
        updates = report["rendering_updates"]["per_process"]["2501"]
        all_statistics = (
            [
                statistics
                for mark in report["statistics"].values()
                for statistics in mark.values()
            ]
            + [
                row[key]
                for row in report["compositions"]["by_combination"]
                for key in row
                if key.endswith("_statistics")
            ]
            + [
                report["rendering"]["vblank_interval_statistics"],
                report["compositions"]["per_process"]["2501"][
                    "active_interval_statistics"
                ],
                updates["update_interval_statistics"],
                updates["refresh_intervals_statistics"],
                updates["latency_statistics"],
            ]
            + list(updates["phase_statistics"].values())
        )

        # Extrapolation used to yield percentiles outside [min, max].
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
        self.assertEqual(
            report["rendering"]["reference_interval_source"], "WaylandFramePresented"
        )
        self.assertEqual(report["compositions"]["compositions"], 2)
        self.assertEqual(report["compositions"]["by_class"]["update"], 2)
        process = report["compositions"]["per_process"]["2501"]
        self.assertEqual(process["replay_mismatches"], 0)
        updates = report["rendering_updates"]["per_process"]["2501"]
        self.assertEqual(
            (updates["updates"], updates["asked"], updates["composited"]), (3, 2, 2)
        )
        self.assertEqual(set(updates["phase_statistics"]), set(UPDATE_PHASES))
        # The old frame cycle section is gone.
        self.assertNotIn("frame_cycle", report)

    def test_analyze_json_update_phases_add_up_to_the_latency(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, format="json", timespan="-", explain=False
        )
        analyze.analyze(args)

        updates = json.loads(self.stdout())["rendering_updates"]["per_process"]["2501"]
        phases_mean = sum(
            statistics["mean"] for statistics in updates["phase_statistics"].values()
        )
        self.assertEqual(
            phases_mean, approx(updates["latency_statistics"]["mean"], rel=1e-6)
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
        # Tile geometry depends on the backend, so only check extraction.
        self.assertEqual(statistics["PaintTile"]["dirty_pixels"]["n"], 80)
        self.assertGreater(statistics["PaintTile"]["dirty_pixels"]["min"], 0)

    def test_analyze_with_a_timespan_holding_vblanks_but_no_rendering_update(self):
        # The window has vblanks, but the first update begins after it.
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE,
            format="text",
            timespan="0-370",
            explain=False,
        )
        analyze.analyze(args)

        stdout = self.stdout()
        self.assertIn("- compositions: 0", stdout)
        self.assertIn(
            "- compositor time:\n  - unknown: 100.0% (before first message", stdout
        )
        self.assertIn("- updates: 0", stdout)

    def test_analyze_json_pairs_an_update_with_a_frame_past_the_timespan(self):
        # The update begins at 371.6 ms and its frame comes at 524.1 ms. Pairing
        # runs over the whole capture.
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE,
            format="json",
            timespan="0-400",
            explain=False,
        )
        analyze.analyze(args)

        report = json.loads(self.stdout())
        self.assertEqual(report["compositions"]["compositions"], 0)
        updates = report["rendering_updates"]["per_process"]["2501"]
        self.assertEqual((updates["updates"], updates["composited"]), (1, 1))

    def test_analyze_keeps_a_composition_whose_marks_trimming_drops(self):
        # The frame at 560.7 ms is inside, but its RenderLayerTree (549.8 to 571.1)
        # is trimmed. Matching runs on the untrimmed capture.
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE,
            format="json",
            timespan="370-561",
            explain=False,
        )
        analyze.analyze(args)

        compositions = json.loads(self.stdout())["compositions"]
        self.assertEqual(compositions["compositions"], 2)
        (row,) = compositions["by_combination"]
        self.assertEqual(row["duration_statistics"]["min"], approx(21.344, abs=0.001))

    def test_analyze_rejects_a_timespan_it_cannot_honour(self):
        for timespan in [
            "abc",
            "0-1e3",
            "1-2-3",
            "5000-",
            "5000-6000",
            "",
            # Empty.
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
        # Rates divide by the analyzed duration.
        self.assertEqual(int(report["document"]["timespan"]["end"]), 4467)
        self.assertEqual(
            report["compositions"]["per_process"]["2501"]["average_frame_rate"],
            approx(0.448, abs=0.001),
        )

    def test_the_report_does_not_depend_on_the_mark_order(self):
        # Grouping by name does not guarantee time order.
        parsed = analyze.parse(SAMPLE_CAPTURE_FILE, marks=True, counters=False)
        data = analyze.sysprof_data_with_marks_by_name(parsed)
        in_parse_order = analyze._prepare_report(data, data)

        for marks in data["marks"].values():
            marks.reverse()
        reversed_order = analyze._prepare_report(data, data)
        for section in ("rendering", "compositions", "rendering_updates"):
            self.assertEqual(reversed_order[section], in_parse_order[section])

    def test_a_capture_of_no_length_reports_no_rate(self):
        # Unlike an empty window, an empty capture is still reported on.
        data = sysprof_data(
            composition(0, 0) + [mark("LayerTreeHostRenderingUpdate", 0, 0)],
            begin_msec=0,
            end_msec=0,
        )

        report = analyze._prepare_report(data, data)

        process = report["compositions"]["per_process"]["1"]
        self.assertIsNone(process["average_frame_rate"])
        self.assertIsNone(process["typical_frame_rate"])
        self.assertIsNone(report["rendering"]["reference_interval"])
        analyze._render_text_report(report)
        self.assertIn(
            "- average frame rate: - (over the analyzed duration)", self.stdout()
        )

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
        self.assertEqual(rendering["vblanks"], 35)
        self.assertEqual(rendering["vblank_interval_statistics"]["n"], 34)

    def test_compositions_are_broken_down_by_their_combination_of_reasons(self):
        marks = []
        for i, reasons in enumerate(
            [
                (ANIMATION,),
                (ANIMATION,),
                (ANIMATION,),
                (RU,),
                (RU, ANIMATION),
                (ANIMATION, RU),
                (),
                None,
            ]
        ):
            marks += composition(10 + i * 20, 11 + i * 20, reasons)
        data = sysprof_data(marks)
        report = analyze._prepare_report(data, data)

        compositions = report["compositions"]
        # By class, then by count.
        self.assertEqual(
            [
                (row["class"], row["combination"], row["n"])
                for row in compositions["by_combination"]
            ],
            [
                ("update", "RenderingUpdate + Animation", 2),
                ("update", "RenderingUpdate", 1),
                ("compositor", "Animation", 3),
                ("no_reason", "no reason", 1),
                ("unrecorded", "unrecorded", 1),
            ],
        )
        # A combined composition counts toward each of its reasons.
        self.assertEqual(compositions["by_reason"], {RU: 3, ANIMATION: 5})

        analyze._render_text_report(report)
        stdout = self.stdout()
        self.assertIn("update-driven: 3, compositor-driven: 3", stdout)
        self.assertIn("- no reason (unexpected): 1", stdout)
        self.assertIn("- unrecorded: 1", stdout)
        self.assertIn(
            "- by reason (a composition with several reasons counts toward each):"
            " RenderingUpdate 3, Animation 5",
            stdout,
        )

    def test_timeline_draws_the_lanes_through_the_command_line(self):
        main(["timeline", "--color", "never", "-n", "5", SAMPLE_CAPTURE_FILE])

        stdout = self.stdout()
        self.assertIn("Timeline: rows of 100.002 ms across the analyzed window", stdout)
        for lane in ("frame", "main", "comp", "tiles"):
            self.assertEqual(stdout.count(" {} \u2595".format(lane)), 5)
        self.assertIn("update 1 at 371.6 ms: frame 1 after 152.490 ms", stdout)
        self.assertIn("style resolution", stdout)

        # Explanations are opt-in.
        self.assertNotIn("How to read the timeline", stdout)

        main(["timeline", "--worst", "update-latency", SAMPLE_CAPTURE_FILE])
        self.assertIn("reached the screen with frame 1", self.stdout())

    def test_timeline_explains_its_terms_through_the_command_line(self):
        main(["timeline", "--color", "never", "-n", "1", "-e", SAMPLE_CAPTURE_FILE])

        stdout = self.stdout()
        self.assertIn("How to read the timeline", stdout)
        self.assertIn(
            "missed refreshes: round(interval / refresh interval) - 1", stdout
        )

    def test_timeline_rejects_options_that_draw_nothing(self):
        for option in [
            ["--resolution", "0"],
            ["--resolution", "-1"],
            # Accepted by float() but fail every comparison.
            ["--resolution", "nan"],
            ["--resolution", "inf"],
            # Rounds to a zero nanosecond cell.
            ["--resolution", "0.0000004"],
            ["--max-rows", "0"],
            ["--max-cells", "0"],
            ["--row-length", "0"],
            ["--row-length", "nan"],
            # Rounds to a zero nanosecond row, which never advances.
            ["--row-length", "0.0000004"],
            ["--frames", "0"],
            ["--frames", "99999"],
            ["--frames", "1", "-t", "0-100"],
            ["--worst", "slowest"],
            ["-t", "0-0"],
            ["-t", "5000-6000"],
        ]:
            with self.subTest(option=option), self.assertRaises(SystemExit):
                main(["timeline"] + option + [SAMPLE_CAPTURE_FILE])

    def test_explain_reaches_the_report_through_the_command_line(self):
        # Through main(), to catch a renamed flag or dest.
        main(["analyze", "-e", SAMPLE_CAPTURE_FILE])

        stdout = self.stdout()
        self.assertIn("A composition is a RenderLayerTree", stdout)
        self.assertIn("StyleRecalc wraps RenderTreeBuild", stdout)

    def test_a_capture_of_its_own_broken_timespan_is_no_usage_error(self):
        # No -t was passed, so the user is not at fault.
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
        self.assertIsNone(extract({"message": "", "duration": 0})["tiles"])

    def test_explaining_an_empty_section_still_explains_it(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE,
            format="text",
            timespan="0-370",
            explain=True,
        )
        analyze.analyze(args)

        stdout = self.stdout()
        self.assertIn("- updates: 0", stdout)
        self.assertIn("A rendering update is a LayerTreeHostRenderingUpdate", stdout)
        self.assertIn("A composition is a RenderLayerTree", stdout)

    def test_a_broken_capture_is_no_usage_error_even_with_a_timespan(self):
        # The capture's own range is at fault, not the user's -t.
        check_timespan_holds_data(0, msec_to_nsec(-5), 0, None)
        check_timespan_holds_data(0, msec_to_nsec(-5), None, None)
        check_timespan_holds_data(0, 0, 0, msec_to_nsec(5))

    def test_a_window_meeting_the_capture_at_one_point_is_rejected(self):
        capture = (msec_to_nsec(100), msec_to_nsec(200))
        # Clamped to the capture, each encloses a single instant.
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
        # Consumers rely on these keys.
        self.assertEqual(sorted(percentiles), ["25", "50", "75", "99"])

    def test_delta_histogram_keeps_the_processes_apart(self):
        # Two processes at 16 ms, offset by 8 ms. Merged, deltas would read 8 ms.
        marks = []
        for i in range(5):
            marks.append(
                mark("LayerTreeHostRenderingUpdate", i * 16, i * 16 + 2, pid=1)
            )
            marks.append(
                mark("LayerTreeHostRenderingUpdate", i * 16 + 8, i * 16 + 10, pid=2)
            )
        data = sysprof_data(marks, end_msec=100)

        deltas = histogram._delta_times_ms(data, "LayerTreeHostRenderingUpdate")
        self.assertEqual(deltas, [approx(16.0)] * 8)

    def test_conditional_lines_only_appear_where_there_is_something_to_say(self):
        clean = sysprof_data(
            steady_animation(100, 5)
            + asking_for_a_frame(300, 310, 309)
            + composition(309.1, 310, (RU,))
            + [frame_complete(320, "InProgress")]
        )
        analyze._render_text_report(analyze._prepare_report(clean, clean))
        stdout = self.stdout()
        for line in [
            "aborted:",
            "replay mismatches:",
            "suspended stretches:",
            "stalled:",
            "uncomposited:",
            "pending at capture end:",
            "pairing:",
            "no reason (unexpected):",
            "unrecorded:",
            "- other:",
        ]:
            self.assertNotIn(line, stdout)

    def test_a_probable_suspension_is_reported(self):
        data = sysprof_data(
            [presented(1), scheduled(10, "Idle", reasons=("Animation",))]
            + composition(50, 51)
            + [frame_complete(60, "InProgress")]
        )
        analyze._render_text_report(analyze._prepare_report(data, data))
        self.assertIn("suspended stretches: 0, probably suspended: 1", self.stdout())

    def test_a_process_with_only_state_messages_reports_its_compositor_time(self):
        data = sysprof_data([scheduled(10, "Idle", reasons=("Animation",))])
        analyze._render_text_report(analyze._prepare_report(data, data))
        self.assertIn("- compositor time:\n  - ", self.stdout())

    def test_a_capture_whose_rlts_all_aborted_says_so(self):
        data = sysprof_data([mark("RenderLayerTree", 10, 11)])
        analyze._render_text_report(analyze._prepare_report(data, data))
        stdout = self.stdout()
        self.assertIn(
            "- no RenderLayerTree of the capture holds a DidRenderFrame", stdout
        )
        self.assertIn("- aborted: 1", stdout)

    def test_conditional_lines_say_what_went_wrong(self):
        broken = sysprof_data(
            steady_animation(100, 5)
            + [mark("RenderLayerTree", 300, 301)]
            + [scheduled(116.2, "Idle", reasons=(ANIMATION,))]
            + asking_for_a_frame(400, 410, 409)
        )
        analyze._render_text_report(analyze._prepare_report(broken, broken))
        stdout = self.stdout()
        self.assertIn("- aborted: 1", stdout)
        self.assertIn("- replay mismatches: 1 of", stdout)
        self.assertIn("messages in the whole capture", stdout)
        self.assertIn("- pending at capture end: 1", stdout)

    def test_several_processes_are_reported_one_by_one(self):
        data = sysprof_data(
            steady_animation(100, 4, pid=1) + steady_animation(108, 4, pid=2)
        )
        report = analyze._prepare_report(data, data)
        analyze._render_text_report(report)
        stdout = self.stdout()
        self.assertIn("- [1] typical frame rate:", stdout)
        self.assertIn("- [2] typical frame rate:", stdout)
        self.assertIn("| pid | class", stdout)
        # Each process's rows share out its own compositions.
        shares = [row["share"] for row in report["compositions"]["by_combination"]]
        self.assertEqual(shares, [1.0, 1.0])
        # Merged, the rate would read doubled.
        for pid in (1, 2):
            self.assertEqual(
                report["compositions"]["per_process"][str(pid)][
                    "active_interval_statistics"
                ]["median"],
                approx(16.0),
            )

    def test_dump_csv_carries_every_column_of_a_row(self):
        args = argparse.Namespace(
            capture_file=SAMPLE_CAPTURE_FILE, marks=True, counters=False, format="csv"
        )
        dump.dump(args)

        header = self.stdout().splitlines()[0]
        self.assertEqual(header, "group;pid;name;message;time;duration;end_time")

    def test_collapsed_stacktraces_resolve_addresses_against_the_process_maps(self):
        # The leaf (first raw address) hits a map. The root is covered by nothing
        # and dropped, as in Sysprof.
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
        # As in Sysprof, rather than shown as a bare address.
        data = stacktrace_data([stacktrace(1, 1, [0x10])], processes={1: "app"})

        self.assertEqual(
            dump._collapsed_stacktraces_to_rows(data), [{"stack": "app", "count": 1}]
        )

    def test_collapsed_stacktraces_drop_the_earlier_of_two_overlapping_maps(self):
        # Like ld-linux-aarch64.so.1 and [vdso] in sample.syscap. find_duplicates()
        # in sysprof-address-layout.c drops the earlier-starting map.
        data = stacktrace_data(
            [stacktrace(1, 1, [0x1050])],
            maps={
                1: [
                    # Dropped.
                    {
                        "start": 0x1000,
                        "end": 0x2000,
                        "offset": 0,
                        "filename": "/lib/loader.so",
                    },
                    # Kept.
                    {"start": 0x1800, "end": 0x1900, "offset": 0, "filename": "[vdso]"},
                ]
            },
            processes={1: "app"},
        )

        self.assertEqual(
            dump._collapsed_stacktraces_to_rows(data), [{"stack": "app", "count": 1}]
        )

    def test_collapsed_stacktraces_offset_by_the_maps_own_file_offset(self):
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
        # A real file on disk, so this goes through webkitsysprof.symbolizer.
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
        # The "__symbols__" bundle was resolved at record time and needs no map.
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
        # The bundle is what sysprof trusted at record time.
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
        # Addresses after PERF_CONTEXT_KERNEL (0xffffffffffffff80) resolve against
        # kallsyms. With no marker back to user space, the boundary is added anyway.
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
        # Kernel unwinds often cannot walk back into user space. Sysprof shows
        # these directly under the process, as here.
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
        # PERF_CONTEXT_USER. As in Sysprof, a marker becomes a pseudo-frame naming
        # the context it leaves, except a leading one. 0x30 is in kernel context.
        data = stacktrace_data(
            [
                stacktrace(
                    1,
                    1,
                    [
                        0xFFFFFFFFFFFFFF80,  # Leading, dropped.
                        0x30,
                        0xFFFFFFFFFFFFFE00,  # Leaves kernel context.
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
        # The shape Sysprof shows for syscall stacks in sample.syscap.
        data = stacktrace_data(
            [
                stacktrace(
                    1,
                    1,
                    [
                        0xFFFFFFFFFFFFFF80,  # Leading, dropped.
                        0x10,
                        0xFFFFFFFFFFFFFE00,  # Leaves kernel context.
                        0x20,  # Unresolved user frame, dropped.
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

        # Sorted, like stackcollapse-perf.pl.
        self.assertEqual(
            dump._collapsed_stacktraces_to_rows(data),
            [
                {"stack": "app;func_a", "count": 2},
                {"stack": "app;func_b", "count": 1},
            ],
        )

    def test_collapsed_stacktraces_collapse_consecutive_identical_frames(self):
        # As in sysprof_document_symbolize_traceable().
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
        # E.g. C1 and C2 constructors. Collapsing compares identity, not text.
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
        # The "corrupted unwind" case of sysprof_callgraph_add_traceable(), a stack
        # holding only PERF_CONTEXT_USER.
        data = stacktrace_data(
            [stacktrace(1, 1, [0xFFFFFFFFFFFFFE00])],
            processes={1: "app"},
        )

        self.assertEqual(
            dump._collapsed_stacktraces_to_rows(data),
            [{"stack": "app;Unwindable", "count": 1}],
        )

    def test_collapsed_stacktraces_do_not_show_unwindable_for_a_lone_kernel_switch(
        self,
    ):
        # sysprof_callgraph_add_traceable() only substitutes for a user context.
        data = stacktrace_data(
            [stacktrace(1, 1, [0xFFFFFFFFFFFFFF80])],
            processes={1: "app"},  # PERF_CONTEXT_KERNEL
        )

        self.assertEqual(
            dump._collapsed_stacktraces_to_rows(data),
            [{"stack": "app;- - Kernel - -", "count": 1}],
        )

    def test_process_name_keeps_a_kernel_thread_s_own_slash(self):
        # A comm is not a path. basename() would merge both into "0".
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
        # One count per SAMPLE frame.
        self.assertEqual(sum(counts), 1026)
        self.assertEqual(stacks, sorted(stacks))
        # From the bundled symbols, with no local binary.
        self.assertIn("WebCore::TextureMapperLayer::paint", stdout)
        self.assertIn("In File /usr/lib/libWPEWebKit-2.0.so.1.10.0", stdout)
        # From the bundled kallsyms.
        self.assertIn("drm_ioctl", stdout)
        # Verified against sysprof-cat. The ld-linux map overlaps [vdso] and is
        # dropped by the address-layout dedup.
        self.assertNotIn("ld-linux", stdout)
        self.assertNotIn(";0x", stdout)
        self.assertIn("kworker/1:2-events", stdout)
        # Matches sysprof-cat's totals.
        self.assertIn("WPENetworkProce;Unwindable 79", stdout)
        self.assertIn("WPEWebProcess;Unwindable 89", stdout)
        self.assertIn("wpe-bare-app;Unwindable 69", stdout)
        # Repeated addresses in one symbol collapse. Adjacent equal text can still
        # occur, e.g. the C1 and C2 ~GraphicsLayerCoordinated() destructors.
        self.assertNotIn(
            "WebCore::GraphicsLayerCoordinated::updateBackingStoresIfNeededv;"
            "WebCore::GraphicsLayerCoordinated::updateBackingStoresIfNeededv",
            stdout,
        )
        # A bundled symbol's nick.
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
        # The display link stops before the window, but its interval still applies.
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
        self.assertEqual(report["rendering"]["reference_interval"], approx(16.0))
        self.assertEqual(
            report["rendering_updates"]["per_process"]["2"][
                "refresh_intervals_statistics"
            ]["median"],
            approx(5 / 16),
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
