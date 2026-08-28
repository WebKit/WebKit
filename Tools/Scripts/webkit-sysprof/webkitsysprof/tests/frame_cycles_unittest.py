"""Frame cycle reconstruction on synthetic captures.

The sample capture only covers cycles that all resolve. These build the awkward
cases by hand.
"""

import unittest

from .helpers import SysprofTestCase, approx, mark, sysprof_data

from webkitsysprof import analyze
from webkitsysprof.cycles import calculate_frame_cycles
from webkitsysprof.utils import (
    display_refreshes,
    median_vblank_interval,
    msec_to_nsec,
)


def frame_cycle_report(data, begin_msec=0, end_msec=1000, vblank_interval=None):
    return analyze._prepare_frame_cycle_report(
        data, msec_to_nsec(begin_msec), msec_to_nsec(end_msec), vblank_interval
    )


class FrameCyclesTest(SysprofTestCase):
    def test_phases_partition_every_resolved_cycle(self):
        data = sysprof_data(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 5),
                mark("RenderLayerTree", 7, 14),
                mark("PaintToGLContext", 10, 13),
                mark("LayerTreeHostRenderingUpdate", 16, 20),
                mark("RenderLayerTree", 22, 30),
                mark("PaintToGLContext", 25, 29),
                mark("LayerTreeHostRenderingUpdate", 32, 36),
            ]
        )

        cycles = calculate_frame_cycles(data)
        self.assertEqual(len(cycles), 2)
        for cycle in cycles:
            self.assertIsNotNone(cycle["phases"])
            self.assertEqual(sum(cycle["phases"].values()), approx(cycle["duration"]))

    def test_composition_ending_after_the_cycle_is_reported_as_an_overrun(self):
        # RenderLayerTree runs on past PaintToGLContext to wait for the GPU to finish
        # the frame, and the next rendering update starts during that wait.
        data = sysprof_data(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 5),
                mark("RenderLayerTree", 7, 30),
                mark("PaintToGLContext", 10, 13),
                mark("WaitForCompositionCompletion", 13, 30),
                mark("LayerTreeHostRenderingUpdate", 20, 25),
            ]
        )

        cycle = calculate_frame_cycles(data)[0]
        self.assertIsNotNone(cycle["phases"])
        # Compositing covers that wait up to the cycle end, the rest counts as an
        # overrun rather than being dropped or booked as idle.
        self.assertEqual(cycle["phases"]["compositing"], approx(13.0))
        self.assertEqual(cycle["phases"]["idle"], approx(0.0))
        self.assertEqual(cycle["composition_overrun"], approx(10.0))
        self.assertEqual(sum(cycle["phases"].values()), approx(cycle["duration"]))

    def test_a_cycle_after_an_overrunning_composition_still_resolves(self):
        # The first cycle composites past the start of the second. Picking the end mark
        # in begin order hands that long one to the second cycle too, which used to drop
        # exactly the slow frames worth looking at.
        data = sysprof_data(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 5),
                mark("RenderLayerTree", 6, 56),
                mark("PaintToGLContext", 10, 50),
                mark("LayerTreeHostRenderingUpdate", 18, 20),
                mark("RenderLayerTree", 22, 23),
                mark("PaintToGLContext", 22, 23),
                mark("LayerTreeHostRenderingUpdate", 30, 35),
            ]
        )

        cycles = calculate_frame_cycles(data)
        self.assertEqual(
            [cycle["phases"] is not None for cycle in cycles], [True, True]
        )
        # The composition begun at 6 ms is still running through the second cycle, so
        # that cycle is compositing from the end of its update to its own end.
        self.assertEqual(cycles[1]["phases"]["compositing"], approx(10.0))
        self.assertEqual(cycles[1]["phases"]["idle"], approx(0.0))
        # Each cycle reports how far its own composition ran past it, so the long one
        # counts for the cycle it began in and the short one for the next.
        self.assertEqual(cycles[0]["composition_overrun"], approx(38.0))
        self.assertEqual(cycles[1]["composition_overrun"], approx(0.0))

    def test_cycles_without_compositing_marks_are_counted_but_not_analyzed(self):
        data = sysprof_data(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 5),
                mark("RenderLayerTree", 7, 14),
                mark("PaintToGLContext", 10, 13),
                mark("LayerTreeHostRenderingUpdate", 16, 20),
                mark("LayerTreeHostRenderingUpdate", 24, 28),
                mark("RenderLayerTree", 30, 36),
                mark("PaintToGLContext", 32, 35),
                mark("LayerTreeHostRenderingUpdate", 40, 44),
            ]
        )

        report = frame_cycle_report(data)
        self.assertEqual(report["cycles"], 3)
        self.assertEqual(report["resolved_duration_statistics"]["n"], 2)
        # The phase statistics cover the two cycles that composited, not all three.
        self.assertEqual(report["resolved_duration_statistics"]["n"], 2)
        self.assertEqual(report["duration_statistics"]["n"], 3)

    def test_the_gap_between_two_rendering_periods_is_reported_as_a_long_cycle(self):
        data = sysprof_data(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 5),
                mark("RenderLayerTree", 7, 14),
                mark("PaintToGLContext", 10, 13),
                mark("LayerTreeHostRenderingUpdate", 16, 20),
                mark("RenderLayerTree", 22, 30),
                mark("PaintToGLContext", 25, 29),
                mark("LayerTreeHostRenderingUpdate", 500, 505),
                mark("RenderLayerTree", 507, 514),
                mark("PaintToGLContext", 510, 513),
                mark("LayerTreeHostRenderingUpdate", 516, 520),
            ]
        )

        report = frame_cycle_report(data)
        # Dropping the 470 ms of idle time between the two bursts would hide it. The
        # median is what the frames took, and the idle spell is the maximum and the
        # idle phase, where it can be read as what it is.
        self.assertEqual(report["cycles"], 3)
        self.assertEqual(report["duration_statistics"]["median"], approx(16.0))
        self.assertEqual(report["duration_statistics"]["max"], approx(484.0))
        self.assertEqual(report["phase_statistics"]["idle"]["max"], approx(470.0))

    def test_the_compositing_mark_name_is_resolved_per_cycle(self):
        # renderLayerTree() opens its trace scope before the checks that can return
        # early, so the first cycle holds a RenderLayerTree with nothing inside it.
        # The marks the second cycle carries must not decide for the first.
        data = sysprof_data(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 5),
                mark("RenderLayerTree", 7, 14),
                mark("LayerTreeHostRenderingUpdate", 16, 20),
                mark("RenderLayerTree", 22, 30),
                mark("FlushCompositingState", 23, 25),
                mark("PaintToGLContext", 25, 29),
                mark("LayerTreeHostRenderingUpdate", 32, 36),
            ]
        )

        cycles = calculate_frame_cycles(data)
        self.assertEqual(
            [cycle["phases"] is not None for cycle in cycles], [True, True]
        )
        self.assertEqual(cycles[0]["phases"]["compositing"], approx(7.0))
        self.assertEqual(cycles[1]["phases"]["compositing"], approx(8.0))

    def test_compositing_is_over_when_the_last_compositing_mark_is(self):
        # A second RenderLayerTree after PaintToGLContext. Consulting only the first
        # mark name that matches would book its 29 ms as idle.
        data = sysprof_data(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 5),
                mark("RenderLayerTree", 6, 10),
                mark("PaintToGLContext", 7, 8),
                mark("RenderLayerTree", 11, 40),
                mark("LayerTreeHostRenderingUpdate", 50, 55),
            ]
        )

        phases = calculate_frame_cycles(data)[0]["phases"]
        # 6 to 10 and 11 to 40, so the millisecond between the two is a wait rather
        # than compositing.
        self.assertEqual(phases["compositing"], approx(33.0))
        self.assertEqual(phases["waiting_for_compositing"], approx(2.0))
        self.assertEqual(phases["idle"], approx(10.0))

    def test_compositing_overlapping_the_rendering_update_still_counts(self):
        # RenderLayerTree starts on the compositing thread 1 ms before the rendering
        # update mark ends. Skipping it would measure the cycle off PaintToGLContext.
        data = sysprof_data(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 10),
                mark("RenderLayerTree", 9, 20),
                mark("PaintToGLContext", 12, 18),
                mark("LayerTreeHostRenderingUpdate", 30, 35),
            ]
        )

        phases = calculate_frame_cycles(data)[0]["phases"]
        self.assertEqual(phases["rendering_update"], approx(10.0))
        self.assertEqual(phases["waiting_for_compositing"], approx(0.0))
        self.assertEqual(phases["compositing"], approx(10.0))
        self.assertEqual(phases["idle"], approx(10.0))

    def test_a_cycle_reaching_past_the_timespan_is_left_out_of_it(self):
        # Like a mark crossing the boundary, which trimming drops: a frame half
        # outside the window must not contribute a whole frame's timing to it.
        data = sysprof_data(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 5),
                mark("RenderLayerTree", 7, 190),
                mark("PaintToGLContext", 10, 180),
                mark("LayerTreeHostRenderingUpdate", 200, 205),
            ],
            end_msec=10,
        )

        report = frame_cycle_report(data, end_msec=10)
        self.assertEqual(report["cycles"], 0)
        self.assertEqual(report["coverage"], approx(0.0))

    def test_a_capture_without_compositing_marks_keeps_its_cycles(self):
        # Rendering updates that composited nothing, so no cycle can be split into
        # phases. The cycles themselves are still measured and reported.
        marks = [
            mark("LayerTreeHostRenderingUpdate", i * 50, i * 50 + 8) for i in range(10)
        ]
        marks += [
            mark("DisplayLinkUpdate", i * 16.67, i * 16.67 + 0.1) for i in range(60)
        ]

        report = frame_cycle_report(sysprof_data(marks))
        self.assertEqual(report["cycles"], 9)
        self.assertEqual(report["resolved_duration_statistics"], {})
        self.assertEqual(report["duration_statistics"]["median"], approx(50.0))

    def test_coverage_is_the_share_of_the_window_the_analyzed_cycles_took(self):
        data = sysprof_data(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 5),
                mark("RenderLayerTree", 7, 90),
                mark("PaintToGLContext", 10, 88),
                mark("LayerTreeHostRenderingUpdate", 100, 105),
                mark("RenderLayerTree", 107, 190),
                mark("PaintToGLContext", 110, 188),
                mark("LayerTreeHostRenderingUpdate", 200, 205),
            ]
        )

        report = frame_cycle_report(data, begin_msec=100, end_msec=300)
        # Only the second cycle lies within the window, and it fills half of it.
        self.assertEqual(report["cycles"], 1)
        self.assertEqual(report["coverage"], approx(0.5))

    def test_a_single_cycle_reports_no_percentiles_rather_than_failing(self):
        # statistics.quantiles() raises below two data points before Python 3.13.
        data = sysprof_data(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 5),
                mark("RenderLayerTree", 7, 14),
                mark("PaintToGLContext", 10, 13),
                mark("LayerTreeHostRenderingUpdate", 16, 20),
            ]
        )

        report = frame_cycle_report(data)
        self.assertEqual(report["cycles"], 1)
        self.assertEqual(report["duration_statistics"]["percentiles"], {})
        self.assertEqual(report["phase_statistics"]["compositing"]["percentiles"], {})

    def test_no_cycles_at_all(self):
        report = frame_cycle_report(
            sysprof_data([mark("LayerTreeHostRenderingUpdate", 0, 5)])
        )
        self.assertEqual(report["cycles"], 0)
        self.assertEqual(report["resolved_duration_statistics"], {})
        self.assertEqual(report["duration_statistics"], {})
        # No cycle is no rate, and reporting 0 would read as an infinitely slow one.
        self.assertIsNone(report["implied_fps"])
        self.assertIsNone(report["vblank_intervals_per_cycle"])

    def test_cycles_per_vblank_interval_is_unknown_without_vblank_marks(self):
        data = sysprof_data(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 5),
                mark("RenderLayerTree", 7, 14),
                mark("PaintToGLContext", 10, 13),
                mark("LayerTreeHostRenderingUpdate", 16, 20),
            ]
        )

        self.assertIsNone(
            median_vblank_interval(display_refreshes(data))
        )
        # Reporting 0 here would read as "the cycle fits in zero vblank intervals".
        self.assertIsNone(frame_cycle_report(data)["vblank_intervals_per_cycle"])

    def test_cycles_are_restricted_to_the_ones_lying_within_the_timespan(self):
        data = sysprof_data(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 5),
                mark("RenderLayerTree", 7, 14),
                mark("PaintToGLContext", 10, 13),
                mark("LayerTreeHostRenderingUpdate", 100, 105),
                mark("RenderLayerTree", 107, 114),
                mark("PaintToGLContext", 110, 113),
                mark("LayerTreeHostRenderingUpdate", 200, 205),
            ]
        )

        self.assertEqual(len(calculate_frame_cycles(data)), 2)
        # 50 ms falls inside the first cycle, so that cycle belongs to neither side.
        self.assertEqual(len(calculate_frame_cycles(data, msec_to_nsec(50), None)), 1)
        self.assertEqual(len(calculate_frame_cycles(data, None, msec_to_nsec(50))), 0)
        self.assertEqual(len(calculate_frame_cycles(data, None, msec_to_nsec(100))), 1)
        self.assertEqual(
            calculate_frame_cycles(data, msec_to_nsec(50), None)[0]["begin_nsec"],
            msec_to_nsec(100),
        )

    def test_compositing_running_inside_the_rendering_update_still_resolves(self):
        # The compositing thread finished while the rendering update mark was still
        # open. There is no compositing phase to report, but the cycle is measured.
        data = sysprof_data(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 100),
                mark("RenderLayerTree", 10, 50),
                mark("LayerTreeHostRenderingUpdate", 200, 205),
            ]
        )

        cycle = calculate_frame_cycles(data)[0]
        self.assertEqual(
            cycle["phases"],
            {
                "rendering_update": approx(100.0),
                "waiting_for_compositing": approx(0.0),
                "compositing": approx(0.0),
                "idle": approx(100.0),
            },
        )

    def test_a_cycle_never_runs_between_two_processes(self):
        # Two web processes rendering at once, which a system-wide capture holds. A
        # cycle spanning both would be a frame of neither.
        marks = []
        for i in range(3):
            marks += [
                mark("LayerTreeHostRenderingUpdate", i * 20, i * 20 + 5, pid=1),
                mark("RenderLayerTree", i * 20 + 6, i * 20 + 12, pid=1),
                mark("LayerTreeHostRenderingUpdate", i * 20 + 10, i * 20 + 15, pid=2),
                mark("RenderLayerTree", i * 20 + 16, i * 20 + 19, pid=2),
            ]

        cycles = calculate_frame_cycles(sysprof_data(marks))
        self.assertEqual(len(cycles), 4)
        self.assertEqual(
            [cycle["duration"] for cycle in cycles], [approx(20.0) for _ in cycles]
        )
        self.assertEqual(sorted({cycle["pid"] for cycle in cycles}), [1, 2])
        # Compositing of one process never lands in the other one's phases.
        for cycle in cycles:
            self.assertEqual(
                cycle["phases"]["compositing"],
                approx(6.0 if cycle["pid"] == 1 else 3.0),
            )

    def test_a_cycle_spent_entirely_on_an_earlier_composition_is_analyzed(self):
        # Compositing that overruns covers the whole of the next cycle, so that cycle
        # holds no compositing mark of its own. Dropping it would leave the phase
        # medians describing the fast frames only.
        data = sysprof_data(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 5),
                mark("RenderLayerTree", 6, 120),
                mark("LayerTreeHostRenderingUpdate", 50, 55),
                mark("LayerTreeHostRenderingUpdate", 100, 105),
                mark("RenderLayerTree", 106, 115),
                mark("LayerTreeHostRenderingUpdate", 150, 155),
            ]
        )

        cycles = calculate_frame_cycles(data)
        self.assertEqual(
            cycles[1]["phases"],
            {
                "rendering_update": approx(5.0),
                "waiting_for_compositing": approx(0.0),
                "compositing": approx(45.0),
                "idle": approx(0.0),
            },
        )
        # The composition overran by 70 ms past the cycle it began in, and is counted
        # there rather than once more in every cycle it runs through.
        self.assertEqual(cycles[0]["composition_overrun"], approx(70.0))
        self.assertEqual(cycles[1]["composition_overrun"], approx(0.0))

    def test_coverage_merges_the_cycles_of_two_processes(self):
        marks = []
        for i in range(10):
            for pid in (1, 2):
                marks += [
                    mark("LayerTreeHostRenderingUpdate", i * 100, i * 100 + 5, pid=pid),
                    mark("RenderLayerTree", i * 100 + 6, i * 100 + 50, pid=pid),
                ]

        report = frame_cycle_report(sysprof_data(marks))
        # Both processes render throughout, so together they cover the 900 ms their
        # cycles span, not the 1800 ms of summing them up.
        self.assertEqual(report["cycles"], 18)
        self.assertEqual(report["coverage"], approx(0.9))

    def test_a_composition_begun_before_the_window_is_still_seen_in_it(self):
        # The stall began at 6 ms, before the window, and holds up the cycle inside it.
        data = sysprof_data(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 5),
                mark("RenderLayerTree", 6, 120),
                mark("LayerTreeHostRenderingUpdate", 50, 55),
                mark("LayerTreeHostRenderingUpdate", 100, 105),
                mark("RenderLayerTree", 106, 115),
                mark("LayerTreeHostRenderingUpdate", 150, 155),
            ]
        )

        windowed = calculate_frame_cycles(data, msec_to_nsec(40), None)
        self.assertEqual(
            [cycle["phases"] is not None for cycle in windowed], [True, True]
        )
        self.assertEqual(windowed[0]["phases"]["compositing"], approx(45.0))

    def test_a_stall_is_seen_past_a_shorter_mark_of_the_same_name(self):
        # One process can emit two marks of one name that overlap, a long composition
        # with a shorter one inside it, and the shorter one must not hide the stall.
        data = sysprof_data(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 2),
                mark("RenderLayerTree", 2, 200),
                mark("RenderLayerTree", 10, 15),
                mark("LayerTreeHostRenderingUpdate", 20, 22),
                mark("LayerTreeHostRenderingUpdate", 40, 42),
            ]
        )

        cycles = calculate_frame_cycles(data)
        self.assertEqual(
            [cycle["phases"] is not None for cycle in cycles], [True, True]
        )
        self.assertEqual(cycles[1]["phases"]["compositing"], approx(18.0))

    def test_a_capture_of_negative_length_reports_no_rate(self):
        data = sysprof_data(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 5),
                mark("RenderLayerTree", 6, 10),
                mark("LayerTreeHostRenderingUpdate", 20, 25),
            ]
        )

        report = analyze._prepare_frame_cycle_report(data, 0, msec_to_nsec(-5), 16.67)
        # A window of less than no length is no duration to be a share of.
        self.assertIsNone(report["coverage"])

    def test_no_cycles_says_so_rather_than_blaming_a_zero_median(self):
        report = frame_cycle_report(
            sysprof_data([mark("LayerTreeHostRenderingUpdate", 10, 15)]),
            vblank_interval=16.67,
        )
        self.assertEqual(report["cycles"], 0)
        # A token, so that rewording the report cannot break a JSON consumer.
        self.assertEqual(report["vblank_intervals_per_cycle_unknown"], "no_cycles")
        self.assertTrue(
            analyze.explanations.UNKNOWN_VBLANK_INTERVALS_PER_CYCLE["no_cycles"]
        )

    def test_every_overrunning_composition_of_a_stall_is_counted(self):
        # Six 16 ms cycles, each compositing 9 ms past its own end. Counting only the
        # first would report a sustained stall as a single slow frame.
        marks = []
        for i in range(6):
            marks += [
                mark("LayerTreeHostRenderingUpdate", i * 16, i * 16 + 3),
                mark("RenderLayerTree", i * 16 + 5, i * 16 + 25),
            ]

        cycles = calculate_frame_cycles(sysprof_data(marks))
        self.assertEqual(
            [cycle["composition_overrun"] for cycle in cycles],
            [approx(9.0) for _ in cycles],
        )

    def test_a_cycle_running_a_sliver_of_an_inherited_composition_is_measured(self):
        # The composition of the first cycle ends a microsecond after the second one's
        # rendering update does, so the second ran a sliver of it.
        data = sysprof_data(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 2),
                mark("RenderLayerTree", 3, 12.001),
                mark("LayerTreeHostRenderingUpdate", 10, 12),
                mark("LayerTreeHostRenderingUpdate", 30, 32),
            ]
        )

        phases = calculate_frame_cycles(data)[1]["phases"]
        self.assertEqual(phases["compositing"], approx(0.001))
        self.assertEqual(phases["idle"], approx(17.999))

    def test_a_cycle_running_only_an_inherited_composition_is_measured(self):
        # The composition began before the cycle and ran through the first half of its
        # rendering update. Nothing began within the cycle, but something composited
        # in it, so it is measured like a cycle whose own composition did the same.
        data = sysprof_data(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 5),
                mark("RenderLayerTree", 10, 100),
                mark("LayerTreeHostRenderingUpdate", 50, 150),
                mark("LayerTreeHostRenderingUpdate", 250, 255),
            ]
        )

        phases = calculate_frame_cycles(data)[1]["phases"]
        self.assertEqual(phases["rendering_update"], approx(100.0))
        self.assertEqual(phases["compositing"], approx(0.0))
        self.assertEqual(phases["idle"], approx(100.0))

    def test_a_cycle_composited_beside_its_update_only_is_measured_as_zero(self):
        # The same capture with the composition ending a microsecond earlier: the
        # second cycle ran it only beside its own rendering update, so it composited
        # for no time of its own. A microsecond of the phase separates the two cases,
        # rather than one of them being measured and the other not.
        data = sysprof_data(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 2),
                mark("RenderLayerTree", 3, 11.999),
                mark("LayerTreeHostRenderingUpdate", 10, 12),
                mark("LayerTreeHostRenderingUpdate", 30, 32),
            ]
        )

        phases = calculate_frame_cycles(data)[1]["phases"]
        self.assertEqual(phases["compositing"], approx(0.0))
        self.assertEqual(phases["idle"], approx(18.0))

    def test_a_mark_ending_before_the_capture_keeps_its_own_end(self):
        # The parser shifts timestamps, so a mark that began before the capture ends
        # at a negative time. Reporting it as ending at 0 would make it look like it
        # was still running when the capture started.
        data = sysprof_data(
            [
                mark("LayerTreeHostRenderingUpdate", -20, -18),
                mark("RenderLayerTree", -19, -17),
                mark("LayerTreeHostRenderingUpdate", 0, 5),
                mark("RenderLayerTree", 6, 12),
                mark("LayerTreeHostRenderingUpdate", 20, 25),
            ]
        )

        cycles = calculate_frame_cycles(data)
        # The first cycle ends before the second begins, so the second waits for its
        # own composition rather than inheriting one that was over long before.
        self.assertEqual(cycles[1]["phases"]["waiting_for_compositing"], approx(1.0))

    def test_a_gap_between_two_compositions_is_a_wait_not_compositing(self):
        for inherited_end in [12.001, 11.999]:
            with self.subTest(inherited_end=inherited_end):
                # The cycle 10 -> 40 runs the tail of the composition it
                # inherited, waits, and then composites again. Reading that as
                # one span from the first mark to the last would report the wait
                # as compositing, and a microsecond either side of the update
                # would move 18 ms between the two phases.
                data = sysprof_data(
                    [
                        mark("LayerTreeHostRenderingUpdate", 0, 2),
                        mark("RenderLayerTree", 3, inherited_end),
                        mark("LayerTreeHostRenderingUpdate", 10, 12),
                        mark("RenderLayerTree", 30, 38),
                        mark("LayerTreeHostRenderingUpdate", 40, 42),
                    ]
                )

                phases = calculate_frame_cycles(data)[1]["phases"]
                self.assertEqual(
                    phases["waiting_for_compositing"], approx(18.0, abs=0.002)
                )
                self.assertEqual(phases["compositing"], approx(8.0, abs=0.002))
                self.assertEqual(phases["idle"], approx(2.0))
                self.assertEqual(sum(phases.values()), approx(30.0))

    def test_two_renderers_of_one_kind_are_told_apart_by_their_process(self):
        # Two web processes rendering every 16 ms, 8 ms apart, both naming themselves
        # "WebKit (Web)". Read by kind they would pair into 8 ms cycles and twice the
        # frame rate of either.
        marks = []
        for i in range(4):
            marks += [
                mark("LayerTreeHostRenderingUpdate", i * 16, i * 16 + 2, pid=1),
                mark("RenderLayerTree", i * 16 + 2, i * 16 + 9, pid=1),
                mark("LayerTreeHostRenderingUpdate", i * 16 + 8, i * 16 + 10, pid=2),
                mark("RenderLayerTree", i * 16 + 10, i * 16 + 15, pid=2),
            ]

        report = frame_cycle_report(sysprof_data(marks, end_msec=200))
        self.assertEqual(report["processes"], 2)
        self.assertEqual(report["duration_statistics"]["median"], approx(16.0))
        self.assertEqual(report["implied_fps"], approx(62.5))

    def test_two_updates_at_one_instant_belong_to_two_processes(self):
        # One process cannot open two updates at once, so a pair beginning together is
        # two of them, and neither has a second update to close a cycle with.
        data = sysprof_data(
            [
                mark("LayerTreeHostRenderingUpdate", 10, 12, pid=1),
                mark("LayerTreeHostRenderingUpdate", 10, 13, pid=2),
            ]
        )

        self.assertEqual(calculate_frame_cycles(data), [])

    def test_a_report_without_a_rate_prints_no_rate(self):
        # implied_fps is None wherever there is no median cycle to take it from, and
        # the renderer must print that rather than formatting a number that is not one.
        frame_cycle = {
            "cycles": 1,
            "processes": 1,
            "duration_statistics": {"n": 1, "min": 0, "max": 0, "median": 0, "mean": 0},
            "resolved_duration_statistics": {},
            "phase_statistics": {phase: {} for phase in analyze.PHASES},
            "overrunning_composition_statistics": {},
            "implied_fps": None,
            "coverage": None,
            "capture_vblank_interval": 16.67,
            "vblank_intervals_per_cycle": None,
            "vblank_intervals_per_cycle_unknown": "zero_length_cycle",
        }

        analyze._render_frame_cycle_numbers(frame_cycle)

        stdout = self.stdout()
        self.assertIn("implied FPS (1000 / median cycle): -", stdout)
        self.assertIn("cycles cover - of the analyzed duration", stdout)

    def test_a_steady_slow_capture_keeps_its_cycles(self):
        # 20 FPS, so every cycle idles for 42 of its 50 ms. That is the rhythm of this
        # capture, not the engine running out of work.
        marks = []
        for i in range(20):
            marks += [
                mark("LayerTreeHostRenderingUpdate", i * 50, i * 50 + 3),
                mark("RenderLayerTree", i * 50 + 3, i * 50 + 8),
            ]

        report = frame_cycle_report(sysprof_data(marks), vblank_interval=16.67)
        self.assertEqual(report["cycles"], 19)
        self.assertEqual(report["implied_fps"], approx(20.0))
        self.assertEqual(report["phase_statistics"]["idle"]["median"], approx(42.0))

    def test_the_composition_overrun_median_describes_the_overrunning_cycles(self):
        marks = []
        for i in range(10):
            begin = i * 100
            # Two of the ten compositions run past the start of the next cycle.
            compositing_end = begin + 150 if i in (3, 7) else begin + 50
            marks += [
                mark("LayerTreeHostRenderingUpdate", begin, begin + 10),
                mark("RenderLayerTree", begin + 20, compositing_end),
            ]
        marks.append(mark("LayerTreeHostRenderingUpdate", 1000, 1010))

        report = frame_cycle_report(sysprof_data(marks, end_msec=2000))
        self.assertEqual(report["overrunning_composition_statistics"]["n"], 2)
        # The eight cycles that did not overrun must not median the number down to 0.
        self.assertEqual(
            report["overrunning_composition_statistics"]["median"], approx(50.0)
        )

    def test_an_unknown_refresh_rate_is_not_reported_as_zero(self):
        report = {
            "document": {"timespan": {"begin": 0.0, "end": 1000.0}},
            "rendering": {
                "frames_rendered": 10,
                "theoretical_fps": 10.0,
                "vblanks": 1,
                "vblank_interval_statistics": {},
            },
        }

        explanation = analyze._theoretical_fps_explanation(report)
        # One mark is not none, and it is no reason to claim a 0 Hz display either.
        self.assertIn("refreshed 1 time,", explanation)
        self.assertNotIn("0.00 Hz", explanation)

    def test_a_zero_median_vblank_interval_is_not_reported_as_shared_timestamps(self):
        report = {
            "document": {"timespan": {"begin": 0.0, "end": 1000.0}},
            "rendering": {
                "frames_rendered": 10,
                "theoretical_fps": 10.0,
                "vblanks": 4,
                # Two of the four share a timestamp, which is enough for a zero median
                # without the marks being the same instant.
                "vblank_interval_statistics": {"median": 0.0},
            },
        }

        explanation = analyze._theoretical_fps_explanation(report)
        self.assertIn("the median interval between two of the 4", explanation)


if __name__ == "__main__":
    unittest.main()
