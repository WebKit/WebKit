"""Tests for the timeline, on synthetic captures.

The default resolution is one cell per millisecond, so a lane reads as its string.
"""

import argparse
import unittest

from .helpers import (
    approx,
    SysprofTestCase,
    composition,
    frame_complete,
    mark,
    presented,
    rendering_update,
    requested,
    scheduled,
    steady_animation,
    sysprof_data,
)

from webkitsysprof.compositions import calculate_compositions
from webkitsysprof.timeline import (
    _Renderer,
    _fitting_resolution,
    _frames_window,
    _time_rows,
    draw_timeline,
)
from webkitsysprof.utils import UsageError, msec_to_nsec

RU = ("RenderingUpdate",)


def _args(**overrides):
    args = argparse.Namespace(
        max_rows=1,
        resolution=1.0,
        max_cells=400,
        tile_lane=True,
        color="never",
        row_length=20.0,
        frames=None,
        worst=None,
        timespan="-",
    )
    for name, value in overrides.items():
        setattr(args, name, value)
    return args


class TimelineTest(SysprofTestCase):
    def setUp(self):
        super().setUp()
        self._read = 0

    def _draw(self, marks, end_msec=1000, **overrides):
        draw_timeline(sysprof_data(marks, 0, end_msec), _args(**overrides))
        # Only this call's output, for cases that draw twice.
        already_read = self._read
        printed = self.stdout()[already_read:]
        self._read += len(printed)
        return printed

    def test_a_process_without_compositor_marks_gets_no_comp_lane(self):
        marks = steady_animation(10, 3, pid=1)
        marks += [rendering_update(0, 1, pid=2), rendering_update(20, 21, pid=2)]
        stdout = self._draw(marks, row_length=100.0, max_rows=2)
        self.assertEqual(stdout.count(" main ▕"), 2)
        self.assertEqual(stdout.count(" frame ▕"), 1)
        self.assertEqual(stdout.count(" comp ▕"), 1)
        # Its rows do not dilute the comp lane's averages.
        alone = self._draw(steady_animation(10, 3, pid=1), row_length=100.0)

        def comp_legend(text):
            start = text.index("lane is the compositor thread")
            return text[start:]

        self.assertEqual(comp_legend(stdout), comp_legend(alone))

    def test_a_long_row_is_drawn_and_its_totals_counted_coarser(self):
        # A 300 s row at a 0.2 ms resolution.
        marks = [rendering_update(0, 2), rendering_update(300000, 300002)]
        stdout = self._draw(
            marks, end_msec=300000, row_length=300000.0, resolution=0.2, max_cells=40
        )
        self.assertIn(" main ", stdout)
        self.assertRegex(stdout, r"idle +2999\d\d\.\d{3} ms")
        # Still a nanosecond resolution draws, painting only the drawn cells.
        self.assertIn(
            " main ", self._draw(steady_animation(100, 3), resolution=0.000001)
        )

    def _lanes(self, marks, **overrides):
        """The bars, as (row label, lane) pairs without the border between them."""
        lanes = []
        for line in self._draw(marks, **overrides).split("\n"):
            if "▕" not in line:
                continue
            label, _, lane = line.partition("▕")
            lanes.append((label.rstrip(), lane.rstrip()))
        return lanes

    def _ruler(self, marks, **overrides):
        lines = self._draw(marks, **overrides).split("\n")
        ticks = next(line for line in lines if line.endswith(" [ms]"))
        return lines[lines.index(ticks) - 1], ticks[: -len(" [ms]")]

    def _refresh_ticks(self, marks, **overrides):
        """The cells of the refresh line under the ruler, None where there is none."""
        stdout = self._draw(marks, **overrides)
        lines = stdout.split("\n")
        refresh = [line for line in lines if line.endswith(" [refresh]")]
        if not refresh:
            return None
        border = next(line for line in lines if "▕" in line).index("▕")
        start, end = border + 1, -len(" [refresh]")
        return refresh[0][start:end]

    def _comp(self, marks, **overrides):
        """The comp lane of the first row."""
        lanes = self._lanes(marks, **overrides)
        return next(lane for label, lane in lanes if label.endswith("comp"))

    def test_a_nested_mark_is_drawn_over_the_one_it_runs_inside(self):
        # Longest first, so the bar shows the call structure.
        lanes = self._lanes(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 10),
                mark("StyleRecalc", 2, 8),
                mark("RenderTreeBuild", 4, 6),
                mark("LayerTreeHostRenderingUpdate", 20, 22),
            ]
        )

        self.assertEqual(lanes[0][1], "uuSSBBSSuu..........")

    def test_the_tile_lane_counts_the_tiles_painted_at_once(self):
        marks = [
            mark("LayerTreeHostRenderingUpdate", 0, 2),
            mark("PaintTile", 12, 14),
            mark("PaintTile", 13, 16),
            mark("LayerTreeHostRenderingUpdate", 20, 22),
        ]

        lanes = self._lanes(marks)

        self.assertEqual(lanes[0][1], "uu..................")
        self.assertEqual(lanes[1][1], "............1211....")
        # It can be left out.
        lanes = self._lanes(marks, tile_lane=False)
        self.assertEqual([lane for _, lane in lanes], ["uu.................."])

    def test_a_bar_is_drawn_from_the_marks_of_its_own_process(self):
        lanes = self._lanes(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 2, pid=1),
                mark("LayerTreeHostRenderingUpdate", 20, 22, pid=1),
                mark("LayerTreeHostRenderingUpdate", 4, 6, pid=2),
                mark("LayerTreeHostRenderingUpdate", 24, 26, pid=2),
                mark("PaintTile", 8, 12, pid=1),
            ],
            max_rows=2,
        )

        # Two lanes per row, each with only its own process's marks.
        main_one, tiles_one, main_two, tiles_two = (lane for _, lane in lanes)
        self.assertEqual(main_one, "uu..................")
        self.assertEqual(main_two, "....uu..............")
        self.assertEqual(tiles_one, "........1111........")
        self.assertEqual(tiles_two, "....................")

    def test_refresh_ticks_are_drawn_under_the_ruler_not_through_the_lanes(self):
        marks = [
            mark("LayerTreeHostRenderingUpdate", 0, 2),
            mark("LayerTreeHostRenderingUpdate", 20, 22),
        ]
        marks += [
            mark("DisplayLinkUpdate", msec, msec, "WebKit (UI)", pid=9)
            for msec in (0, 8, 16, 24)
        ]

        # Ticks mark vblanks, not refresh-length steps from the row begin.
        self.assertEqual(self._lanes(marks)[0][1], "uu..................")
        self.assertEqual(self._refresh_ticks(marks), "        ^       ^   ")
        # A bar padded to the refresh interval still shows the tick.
        marks = [
            mark("LayerTreeHostRenderingUpdate", 0, 1),
            mark("LayerTreeHostRenderingUpdate", 4, 5),
        ]
        marks += [
            mark("DisplayLinkUpdate", msec, msec, "WebKit (UI)", pid=9)
            for msec in (0, 8, 16, 24)
        ]
        self.assertEqual(
            self._lanes(marks, row_length=4.0, tile_lane=False)[0][1], "u..."
        )
        self.assertEqual(
            self._refresh_ticks(marks, row_length=4.0, tile_lane=False), "        ^"
        )

    def test_an_interval_of_too_few_cells_draws_no_tick(self):
        # With too few cells per interval the tick line would be meaningless.
        marks = [
            mark("LayerTreeHostRenderingUpdate", 0, 1),
            mark("LayerTreeHostRenderingUpdate", 150, 151),
        ]
        marks += [
            mark("DisplayLinkUpdate", msec, msec, "WebKit (UI)", pid=9)
            for msec in (0, 8, 16, 24)
        ]

        for resolution in (50.0, 8.0, 4.0):
            with self.subTest(resolution=resolution):
                stdout = self._draw(
                    marks, resolution=resolution, row_length=150.0, tile_lane=False
                )

                self.assertIn("fewer than 4 cells, so no refresh ticks", stdout)
                self.assertNotIn("[refresh]", stdout)

    def test_a_capture_without_refreshes_says_the_interval_is_unknown(self):
        stdout = self._draw(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 1),
                mark("LayerTreeHostRenderingUpdate", 20, 21),
            ],
            tile_lane=False,
        )

        self.assertIn("Refresh interval: unknown", stdout)
        self.assertNotIn("0.0000 ms", stdout)
        self.assertNotIn("[refresh]", stdout)

    def test_a_bar_says_which_process_rendered_it(self):
        marks = [
            mark("LayerTreeHostRenderingUpdate", 0, 2, pid=1),
            mark("LayerTreeHostRenderingUpdate", 20, 22, pid=1),
            mark("LayerTreeHostRenderingUpdate", 4, 6, pid=2),
            mark("LayerTreeHostRenderingUpdate", 24, 26, pid=2),
        ]

        lanes = self._lanes(marks, tile_lane=False, max_rows=2)

        self.assertIn("[1]", lanes[0][0])
        self.assertIn("[2]", lanes[1][0])
        # One renderer needs no process on its bars.
        lanes = self._lanes(marks[:2], tile_lane=False)
        self.assertNotIn("[1]", lanes[0][0])

    def test_every_lane_starts_in_the_same_column_as_the_ruler(self):
        # Labels naming a process are wider, and a long duration must not widen
        # them either.
        long_row = [
            mark("LayerTreeHostRenderingUpdate", 0, 2),
            mark("LayerTreeHostRenderingUpdate", 120020, 120022),
        ]
        long_options = dict(
            end_msec=130000,
            row_length=130000.0,
            resolution=10.0,
            max_cells=8,
            tile_lane=False,
        )
        cases = [
            (self._two_processes(1, 1), {"max_rows": 2}),
            (self._two_processes(1, 2), {"max_rows": 2}),
            (long_row + steady_animation(10, 2), long_options),
        ]
        for marks, options in cases:
            with self.subTest(options=options, marks=len(marks)):
                stdout = self._draw(marks, **options)
                lines = [line for line in stdout.split("\n") if "▕" in line]
                columns = {line.index("▕") for line in lines}
                self.assertEqual(len(columns), 1)

                labels, ticks = self._ruler(marks, **options)
                self.assertEqual(ticks.index("┼"), columns.pop() + 1)
                self.assertEqual(labels.index("0"), ticks.index("┼"))

    @staticmethod
    def _two_processes(first, second):
        return [
            mark("LayerTreeHostRenderingUpdate", 0, 2, pid=first),
            mark("LayerTreeHostRenderingUpdate", 20, 22, pid=first),
            mark("LayerTreeHostRenderingUpdate", 4, 6, pid=second),
            mark("LayerTreeHostRenderingUpdate", 24, 26, pid=second),
        ]

    def test_the_ruler_labels_never_run_into_one_another(self):
        marks = [
            mark("LayerTreeHostRenderingUpdate", 0, 2),
            mark("LayerTreeHostRenderingUpdate", 40, 42),
        ]

        for resolution in (1.0, 2.0, 0.2):
            with self.subTest(resolution=resolution):
                labels, _ = self._ruler(
                    marks, resolution=resolution, row_length=40.0, tile_lane=False
                )
                # Labels never run together.
                for label in labels.split():
                    self.assertTrue(label.isdigit(), labels)

    def test_the_last_ruler_label_is_not_cut_at_the_row_end(self):
        marks = [
            mark("LayerTreeHostRenderingUpdate", 0, 2),
            mark("LayerTreeHostRenderingUpdate", 100, 102),
        ]
        labels, ticks = self._ruler(
            marks, resolution=0.5, row_length=100.5, tile_lane=False
        )
        positions = [i for i, tick in enumerate(ticks) if tick == "┼"]
        read = [labels[i:].split()[0] for i in positions]
        self.assertEqual(read[-1], "100", labels)
        self.assertEqual(
            [int(label) for label in read],
            list(range(0, 101, int(read[1]))),
        )

    def test_colour_runs_are_one_escape_and_the_padding_is_blank(self):
        lanes = self._lanes(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 10),
                mark("LayerTreeHostRenderingUpdate", 20, 22),
            ],
            color="always",
            tile_lane=False,
        )

        # One escape per colour run, not per cell.
        self.assertEqual(lanes[0][1].count("\x1b[38;5;"), 2)
        self.assertEqual(lanes[0][1].count("\x1b[0m"), 1)

        marks = [
            mark("LayerTreeHostRenderingUpdate", 0, 1),
            mark("LayerTreeHostRenderingUpdate", 4, 5),
        ]
        marks += [
            mark("DisplayLinkUpdate", msec, msec, "WebKit (UI)", pid=9)
            for msec in (0, 8, 16, 24)
        ]

        lanes = self._lanes(marks, row_length=4.0, color="always")

        # Blank like the tiles lane, not a colour that reads as idle time.
        for _, lane in lanes:
            self.assertEqual(lane.count("█"), 4, repr(lane))
            self.assertTrue(lane.endswith("\x1b[0m"), repr(lane))

    def test_a_capture_without_compositor_or_update_marks_says_what_it_needs(self):
        self.assertIn("no compositor marks", self._draw([mark("StyleRecalc", 0, 2)]))

    def test_the_legend_counts_the_part_of_a_row_the_bar_could_not_fit(self):
        # The bar is cut to the cell limit but the legend is not, since the tail
        # of a long row is what matters.
        stdout = self._draw(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 2),
                mark("StyleRecalc", 10, 20),
                mark("LayerTreeHostRenderingUpdate", 20, 22),
            ],
            max_cells=8,
            tile_lane=False,
        )

        # None of the 10 ms of style resolution was drawn.
        self.assertIn("▕uu......»", stdout)
        legend = next(line for line in stdout.split("\n") if "style resolution" in line)
        self.assertIn("10.000 ms", legend)

    def test_a_compositor_frame_is_drawn_with_its_display_round_trip(self):
        marks = [rendering_update(0, 1), rendering_update(40, 41)]
        marks += [scheduled(9.5, "Idle", reasons=("Animation",))]
        marks += composition(10, 13)
        marks += [
            scheduled(10.3, "InProgress", reasons=("Animation",)),
            frame_complete(20.5, "ScheduledWhileInProgress", reasons=("Animation",)),
        ]
        marks += composition(21, 24)
        marks += [frame_complete(30.5, "InProgress")]

        lanes = self._lanes(marks, row_length=40.0)

        self.assertEqual(
            [label.split()[-1] for label, _ in lanes],
            ["frame", "main", "comp", "tiles"],
        )
        self.assertEqual(lanes[1][1], "u" + "." * 39)
        # Unknown before the first message, scheduled, an Animation composition
        # with its frame, its display round trip, and idle after the last one.
        self.assertEqual(
            lanes[2][1],
            "x" * 9 + "-" + "a!a" + "=" * 7 + "-" + "a!a" + "=" * 6 + "." * 10,
        )
        self.assertIn("2 frames: Animation 2", self.stdout())

    def test_an_update_frame_is_drawn_from_the_compositor_marks(self):
        marks = [rendering_update(0, 9), requested(8.01), rendering_update(40, 41)]
        marks += [scheduled(8, "Idle", reasons=("RenderingUpdate",))]
        marks += composition(10, 18, ("RenderingUpdate",), frame_at=15)
        marks += [
            mark("SkiaBackingStoreTileUpdate", 11, 12.5),
            mark("WaitForCompositionCompletion", 15, 18),
            frame_complete(25, "InProgress"),
        ]

        lanes = self._lanes(marks, row_length=40.0, tile_lane=False)

        self.assertEqual(lanes[2][1], "x" * 8 + "--" + "fUUPP#ww" + "=" * 7 + "." * 15)
        # Tile uploads run on the compositor thread, so the main lane has none.
        self.assertNotIn("U", lanes[1][1])

    def test_a_rendering_update_waiting_for_tiles_blocks_the_compositor(self):
        marks = [rendering_update(0, 9), requested(8.01), rendering_update(40, 41)]
        marks += [
            scheduled(8, "Idle", waiting=True, reasons=("RenderingUpdate",)),
            scheduled(20, "Scheduled", reasons=("RenderingUpdate",)),
        ]
        marks += composition(21, 22, ("RenderingUpdate",))
        marks += [frame_complete(30, "InProgress")]

        self.assertIn(
            "x" * 8 + ":" * 12 + "-",
            self._comp(marks, row_length=40.0, tile_lane=False),
        )

    def test_compositions_without_an_update_are_drawn_by_their_reasons(self):
        marks = [rendering_update(0, 1), rendering_update(40, 41)]
        marks += composition(10, 13, ("VideoFrame", "Animation"))
        marks += composition(20, 23, ())
        marks += composition(30, 33, None)

        comp = self._comp(marks, row_length=40.0, tile_lane=False)

        self.assertIn("v!v", comp)
        self.assertIn("n!n", comp)
        self.assertIn("?!?", comp)
        # Several compositor reasons are a shade of their own.
        colored = self._comp(marks, row_length=40.0, tile_lane=False, color="always")
        self.assertIn("\x1b[38;5;110m", colored)

    def test_the_refresh_ticks_need_no_display_link(self):
        marks = [rendering_update(0, 1), rendering_update(40, 41), presented(5, 8.0)]

        stdout = self._draw(marks, row_length=40.0, tile_lane=False)

        self.assertIn("(from WaylandFramePresented)", stdout)
        self.assertEqual(
            self._refresh_ticks(marks, row_length=40.0, tile_lane=False),
            "        ^       ^       ^       ^       ",
        )

    def test_rows_are_drawn_per_process(self):
        marks = steady_animation(100, 4, pid=1) + steady_animation(108, 4, pid=2)

        lanes = self._lanes(marks, row_length=50.0, max_rows=10, tile_lane=False)

        labels = [label for label, _ in lanes if label.endswith("frame")]
        self.assertTrue(any("[1]" in label for label in labels))
        self.assertTrue(any("[2]" in label for label in labels))
        self.assertIn("Timeline: rows of 50.000 ms", self.stdout())
        # Also for a process whose RLTs all aborted.
        stdout = self._draw(
            [scheduled(10, "Idle", reasons=("Animation",))]
            + [mark("RenderLayerTree", 10.1, 11)],
        )
        self.assertIn(" comp ", stdout)

    def _frame_lane(self, marks, **overrides):
        lanes = self._lanes(marks, tile_lane=False, **overrides)
        return next(lane for label, lane in lanes if label.endswith("frame"))

    def test_the_frame_lane_numbers_each_frame_at_its_cell(self):
        # Frames at 10.5, 12.5, ... 32.5 ms, 2 cells apart, so 10 fills its gap
        # and 11 gets a bare tick.
        lane = self._frame_lane(steady_animation(10, 12, period=2.0), row_length=40.0)
        self.assertEqual(lane, " " * 10 + "1 2 3 4 5 6 7 8 9 10| 12")
        # A frame a refresh late, the compositor active, is marked.
        marks = steady_animation(10, 3, period=40.0) + [presented(0, 16.0)]
        lane = self._frame_lane(marks, row_length=100.0)
        self.assertEqual(lane, " " * 10 + "1" + " " * 39 + "2*" + " " * 38 + "3*")
        # Frames are numbered over the capture, not the row.
        self.assertIn(
            "frame ▕          2*", self._draw(marks, row_length=40.0, max_rows=3)
        )

    def test_each_row_lists_the_updates_it_shows_and_their_frames(self):
        marks = [rendering_update(0, 9), requested(8.01), rendering_update(40, 41)]
        marks += [scheduled(8, "Idle", reasons=RU)]
        marks += composition(10, 18, RU, frame_at=15)
        stdout = self._draw(marks, row_length=20.0, max_rows=3, tile_lane=False)
        self.assertIn("update 1 at 0.0 ms: frame 1 after 15.000 ms", stdout)
        self.assertIn("update 2 at 40.0 ms: asked for no frame", stdout)
        # A row the update began before still lists it where its frame lands.
        stdout = self._draw(marks, row_length=12.0, max_rows=2)
        self.assertEqual(stdout.count("update 1 at 0.0 ms: frame 1"), 2)
        # But not where it only passes through.
        stdout = self._draw(marks, row_length=4.0, max_rows=4)
        self.assertEqual(stdout.count("update 1 at 0.0 ms: frame 1"), 2)

    def test_the_worst_rows_are_drawn_around_the_worst_events(self):
        # 40 ms intervals in one process and 20 ms ones in the other.
        marks = steady_animation(100, 3, period=40.0, pid=1)
        marks += steady_animation(100, 3, period=20.0, pid=2)
        stdout = self._draw(marks, worst="frame-interval", row_length=100.0, max_rows=9)
        notes = [line for line in stdout.split("\n") if line.startswith("frame ")]
        # Frames are numbered over both processes. The second interval of each
        # lies in the row of the first.
        self.assertEqual(
            notes,
            [
                "frame 4 came 40.000 ms after frame 1",
                "frame 3 came 20.000 ms after frame 2",
            ],
        )
        self.assertIn("worst first, 2 of 2 shown", stdout)

        marks = [rendering_update(0, 9), requested(8.01)]
        marks += [scheduled(8, "Idle", reasons=RU)]
        marks += composition(10, 18, RU, frame_at=15)
        stdout = self._draw(marks, worst="update-latency", row_length=100.0)
        self.assertIn(
            "update 1 reached the screen with frame 1 after 15.000 ms", stdout
        )
        self.assertIn(
            "No active frame intervals",
            self._draw(marks, worst="frame-interval"),
        )

    def test_a_worst_row_longer_than_the_cells_draws_coarser(self):
        self.assertEqual(_fitting_resolution(300.0, 400), 1.0)
        self.assertEqual(_fitting_resolution(520.0, 400), 2.0)
        self.assertEqual(_fitting_resolution(3000.0, 400), 10.0)


class FramesWindowTest(unittest.TestCase):
    def test_frames_select_the_window_from_the_first_to_the_last(self):
        data = sysprof_data(steady_animation(100, 5), 0, 1000)
        model = calculate_compositions(data)
        # From the first frame's composition to the last frame, 10 ms either side.
        self.assertEqual(
            _frames_window(data, model, "2-4", 100.0),
            (msec_to_nsec(106), msec_to_nsec(158.5)),
        )
        # A row centered on a single frame.
        self.assertEqual(
            _frames_window(data, model, "3", 20.0),
            (msec_to_nsec(122.5), msec_to_nsec(142.5)),
        )
        for frames in ["6", "0", "4-2", "a", "1-2-3"]:
            with self.subTest(frames=frames), self.assertRaises(UsageError):
                _frames_window(data, model, frames, 100.0)


class RowsTest(unittest.TestCase):
    def test_too_many_rows_are_refused_before_they_are_built(self):
        model = calculate_compositions(sysprof_data(steady_animation(100, 3), 0, 1000))
        window = (0, msec_to_nsec(1000))
        self.assertEqual(len(_time_rows([1], model, 100.0, window)), 10)
        with self.assertRaises(UsageError):
            _time_rows([1], model, 0.0001, window)

    def test_cells_are_measured_in_the_whole_nanoseconds_they_index(self):
        data = sysprof_data(steady_animation(100, 3), 0, 1000)
        model = calculate_compositions(data)
        renderer = _Renderer(data, _args(), model, [], 100.0, 0.0000015)
        self.assertEqual(renderer.resolution, 2)
        self.assertEqual(renderer.cell_msec, 0.000002)
        self.assertEqual(renderer.reference_cells, approx(model.reference / 0.000002))

    def test_a_partial_last_cell_counts_in_part(self):
        data = sysprof_data(steady_animation(100, 3), 0, 1000)
        model = calculate_compositions(data)
        renderer = _Renderer(data, _args(), model, [], 100.0, 0.2)
        totals = {}
        renderer._count(totals, ["a"] * 80 + ["b"], 16.05)
        self.assertEqual(totals["a"], approx(16.0))
        self.assertEqual(totals["b"], approx(0.05))


if __name__ == "__main__":
    unittest.main()
