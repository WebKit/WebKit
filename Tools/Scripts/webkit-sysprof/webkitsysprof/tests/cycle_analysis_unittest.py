"""Drawing one bar per frame cycle, on synthetic captures.

Every case fixes the resolution at one cell per millisecond, so a lane can be
written down as the string it is expected to print.
"""

import argparse
import unittest

from .helpers import SysprofTestCase, mark, sysprof_data

from webkitsysprof.cycle_analysis import draw_frame_cycles


class CycleAnalysisTest(SysprofTestCase):
    def setUp(self):
        super().setUp()
        self._read = 0

    def _draw(self, marks, **overrides):
        args = argparse.Namespace(
            max_cycles=40,
            resolution=1.0,
            order="first",
            min_duration=0.0,
            min_length=0.0,
            max_cells=400,
            tile_lane=True,
            color="never",
        )
        for name, value in overrides.items():
            setattr(args, name, value)
        draw_frame_cycles(sysprof_data(marks), args)
        # Only what this call printed: the capture keeps everything the test wrote,
        # and a case drawing twice would otherwise read the first bars back too.
        already_read = self._read
        printed = self.stdout()[already_read:]
        self._read += len(printed)
        return printed

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

    def test_a_nested_mark_is_drawn_over_the_one_it_runs_inside(self):
        # Painted longest first, so the bar reads as the call structure rather than
        # as whichever mark the walk happened to reach last.
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
        # The main thread sits idle while the painting threads work, which is what
        # the second lane is for.
        lanes = self._lanes(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 2),
                mark("PaintTile", 12, 14),
                mark("PaintTile", 13, 16),
                mark("LayerTreeHostRenderingUpdate", 20, 22),
            ]
        )

        self.assertEqual(lanes[0][1], "uu..................")
        self.assertEqual(lanes[1][1], "............1211....")

    def test_the_tile_lane_can_be_left_out(self):
        lanes = self._lanes(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 2),
                mark("PaintTile", 12, 14),
                mark("LayerTreeHostRenderingUpdate", 20, 22),
            ],
            tile_lane=False,
        )

        self.assertEqual([lane for _, lane in lanes], ["uu.................."])

    def test_a_bar_is_drawn_from_the_marks_of_its_own_process(self):
        # Two renderers render independently of one another, so painting the marks
        # of one into the bar of the other invents work it never did.
        lanes = self._lanes(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 2, pid=1),
                mark("LayerTreeHostRenderingUpdate", 20, 22, pid=1),
                mark("LayerTreeHostRenderingUpdate", 4, 6, pid=2),
                mark("LayerTreeHostRenderingUpdate", 24, 26, pid=2),
                mark("PaintTile", 8, 12, pid=1),
            ]
        )

        # Two lanes per cycle. Neither bar carries the other's rendering update,
        # and the tiles painted for the first process stay out of the second's lane.
        main_one, tiles_one, main_two, tiles_two = (lane for _, lane in lanes)
        self.assertEqual(main_one, "uu..................")
        self.assertEqual(main_two, "uu..................")
        self.assertEqual(tiles_one, "........1111........")
        self.assertEqual(tiles_two, "....................")

    def test_a_refresh_marker_crosses_every_bar_at_each_interval(self):
        marks = [
            mark("LayerTreeHostRenderingUpdate", 0, 2),
            mark("LayerTreeHostRenderingUpdate", 20, 22),
        ]
        marks += [
            mark("DisplayLinkUpdate", msec, msec, "WebKit (UI)", pid=9)
            for msec in (0, 8, 16, 24)
        ]

        lanes = self._lanes(marks)

        # A cycle reaching past the first marker is a frame the display never saw.
        self.assertEqual(lanes[0][1], "uu......│.......│...")

    def test_a_bar_padded_to_the_refresh_interval_still_shows_the_marker(self):
        # The padding exists to keep the marker visible on a short cycle, so a bar
        # of exactly one interval that cannot fit one is the case it is for.
        marks = [
            mark("LayerTreeHostRenderingUpdate", 0, 1),
            mark("LayerTreeHostRenderingUpdate", 4, 5),
        ]
        marks += [
            mark("DisplayLinkUpdate", msec, msec, "WebKit (UI)", pid=9)
            for msec in (0, 8, 16, 24)
        ]

        lanes = self._lanes(marks, min_length=8.0, tile_lane=False)

        self.assertEqual(lanes[0][1], "u...    │")

    def test_an_interval_of_too_few_cells_draws_no_marker(self):
        # At one cell per interval every cell but the first is a marker, and below a
        # cell there is none to put one in at all, so neither draws a lane worth
        # reading and both say so.
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
                stdout = self._draw(marks, resolution=resolution, tile_lane=False)

                self.assertIn("fewer than 4 cells, so no refresh marker", stdout)
                self.assertNotIn("│", stdout)

    def test_a_capture_without_refreshes_says_the_interval_is_unknown(self):
        stdout = self._draw(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 1),
                mark("LayerTreeHostRenderingUpdate", 20, 21),
            ],
            tile_lane=False,
        )

        # Rather than an interval of 0.0000 ms and a marker that never appears.
        self.assertIn("Refresh interval: unknown", stdout)
        self.assertNotIn("0.0000 ms", stdout)
        self.assertNotIn("│", stdout)

    def test_the_slowest_cycles_are_the_ones_worth_drawing(self):
        marks = [
            mark("LayerTreeHostRenderingUpdate", 0, 2),
            mark("LayerTreeHostRenderingUpdate", 10, 12),
            mark("LayerTreeHostRenderingUpdate", 60, 62),
            mark("LayerTreeHostRenderingUpdate", 70, 72),
        ]

        lanes = self._lanes(marks, order="slowest", max_cycles=1, tile_lane=False)

        # The 50 ms cycle, not the 10 ms ones either side, and it keeps its number.
        self.assertEqual(len(lanes), 1)
        self.assertIn("#2", lanes[0][0])
        self.assertIn("50.00 ms", lanes[0][0])

    def test_a_cycle_shorter_than_the_minimum_is_left_out(self):
        marks = [
            mark("LayerTreeHostRenderingUpdate", 0, 2),
            mark("LayerTreeHostRenderingUpdate", 10, 12),
            mark("LayerTreeHostRenderingUpdate", 60, 62),
        ]

        lanes = self._lanes(marks, min_duration=20.0, tile_lane=False)

        self.assertEqual(len(lanes), 1)
        self.assertIn("#2", lanes[0][0])

    def test_a_bar_says_which_process_rendered_it(self):
        marks = [
            mark("LayerTreeHostRenderingUpdate", 0, 2, pid=1),
            mark("LayerTreeHostRenderingUpdate", 20, 22, pid=1),
            mark("LayerTreeHostRenderingUpdate", 4, 6, pid=2),
            mark("LayerTreeHostRenderingUpdate", 24, 26, pid=2),
        ]

        lanes = self._lanes(marks, tile_lane=False)

        self.assertIn("[1]", lanes[0][0])
        self.assertIn("[2]", lanes[1][0])

    def test_one_renderer_needs_no_process_on_its_bars(self):
        lanes = self._lanes(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 2),
                mark("LayerTreeHostRenderingUpdate", 20, 22),
            ],
            tile_lane=False,
        )

        self.assertNotIn("[1]", lanes[0][0])

    def test_every_lane_starts_in_the_same_column_as_the_ruler(self):
        # The label of a bar naming its process is wider than one that does not, and
        # a ruler indented past a narrower label points at the wrong cell.
        for pids in ([1, 1], [1, 2]):
            with self.subTest(pids=pids):
                marks = [
                    mark("LayerTreeHostRenderingUpdate", 0, 2, pid=pids[0]),
                    mark("LayerTreeHostRenderingUpdate", 20, 22, pid=pids[0]),
                    mark("LayerTreeHostRenderingUpdate", 4, 6, pid=pids[1]),
                    mark("LayerTreeHostRenderingUpdate", 24, 26, pid=pids[1]),
                ]
                stdout = self._draw(marks)
                lines = [line for line in stdout.split("\n") if "▕" in line]
                columns = {line.index("▕") for line in lines}
                self.assertEqual(len(columns), 1)

                labels, ticks = self._ruler(marks)
                # The first tick names the first cell, which follows the border.
                self.assertEqual(ticks.index("┼"), columns.pop() + 1)
                self.assertEqual(labels.index("0"), ticks.index("┼"))

    def test_the_ruler_labels_never_run_into_one_another(self):
        marks = [
            mark("LayerTreeHostRenderingUpdate", 0, 2),
            mark("LayerTreeHostRenderingUpdate", 40, 42),
        ]

        for resolution in (1.0, 2.0, 0.2):
            with self.subTest(resolution=resolution):
                labels, _ = self._ruler(marks, resolution=resolution, tile_lane=False)
                # Every label is a number followed by at least one space, so no two
                # of them can be read as one.
                for label in labels.split():
                    self.assertTrue(label.isdigit(), labels)

    def test_the_padding_past_a_cycle_is_blank_in_colour_too(self):
        marks = [
            mark("LayerTreeHostRenderingUpdate", 0, 1),
            mark("LayerTreeHostRenderingUpdate", 4, 5),
        ]
        marks += [
            mark("DisplayLinkUpdate", msec, msec, "WebKit (UI)", pid=9)
            for msec in (0, 8, 16, 24)
        ]

        lanes = self._lanes(marks, min_length=8.0, color="always")

        # Not a block of its own colour, which reads as more idle time inside the
        # cycle, and the same blank the tiles lane draws for that stretch.
        main, tiles = lanes[0][1], lanes[1][1]
        self.assertTrue(main.endswith("    \x1b[38;5;231m│\x1b[0m"), repr(main))
        self.assertTrue(tiles.endswith("    \x1b[38;5;231m│\x1b[0m"), repr(tiles))

    def test_a_run_of_one_colour_is_written_as_one_escape(self):
        lanes = self._lanes(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 10),
                mark("LayerTreeHostRenderingUpdate", 20, 22),
            ],
            color="always",
            tile_lane=False,
        )

        # Ten cells of update and ten of idle, so two colours and one reset rather
        # than an escape pair around every cell.
        self.assertEqual(lanes[0][1].count("\x1b[38;5;"), 2)
        self.assertEqual(lanes[0][1].count("\x1b[0m"), 1)

    def test_a_bar_longer_than_the_cell_limit_is_marked_as_truncated(self):
        lanes = self._lanes(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 2),
                mark("LayerTreeHostRenderingUpdate", 20, 22),
            ],
            max_cells=8,
            tile_lane=False,
        )

        self.assertEqual(lanes[0][1], "uu......»")

    def test_a_capture_without_cycles_says_which_marks_it_needs(self):
        # No window was given, so the capture itself is what holds no cycles, and
        # that is a different thing to tell the reader than a window that cut them.
        self.assertIn(
            "needs at least two rendering updates",
            self._draw([mark("LayerTreeHostRenderingUpdate", 0, 2)]),
        )

    def test_the_legend_counts_the_cycle_the_bar_could_not_fit(self):
        # The bar is cut to the cell limit, the average printed under it is not:
        # counting the drawn cells alone drops the tail of every long cycle, which
        # is the part worth hunting.
        stdout = self._draw(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 2),
                mark("StyleRecalc", 10, 20),
                mark("LayerTreeHostRenderingUpdate", 20, 22),
            ],
            max_cells=8,
            tile_lane=False,
        )

        # All ten milliseconds of style resolution, none of which was drawn.
        self.assertIn("▕uu......»", stdout)
        legend = next(line for line in stdout.split("\n") if "style resolution" in line)
        self.assertIn("10.000 ms", legend)

    def test_padding_that_did_not_fit_is_no_truncated_cycle(self):
        # The padding is not part of the cycle, so a bar that lost nothing but
        # padding has all of its cycle in it and must not say otherwise.
        stdout = self._draw(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 2),
                mark("LayerTreeHostRenderingUpdate", 5, 7),
            ],
            min_length=100.0,
            max_cells=8,
            tile_lane=False,
        )

        self.assertIn("▕uu...", stdout)
        self.assertNotIn("»", stdout)

    def test_the_next_cycle_is_left_out_of_this_ones_last_cell(self):
        # A cycle that is not a whole number of cells long ends inside its last one,
        # and what runs there afterwards belongs to the cycle after it.
        lanes = self._lanes(
            [
                mark("LayerTreeHostRenderingUpdate", 0, 0.5),
                mark("LayerTreeHostRenderingUpdate", 2.5, 4.5),
                mark("LayerTreeHostRenderingUpdate", 10, 11),
            ],
            tile_lane=False,
        )

        # Three cells for the 2.5 ms cycle, and the update beginning at 2.5 ms is
        # drawn in the bar of the cycle it starts, not in the last cell of this one.
        self.assertEqual(lanes[0][1], "u..")

    def test_a_bar_stays_in_its_column_however_long_the_cycle_is(self):
        # A cycle long enough that its duration outgrows the label field would push
        # its own bar out of the column the ruler names the cells in.
        marks = [
            mark("LayerTreeHostRenderingUpdate", 0, 2),
            mark("LayerTreeHostRenderingUpdate", 20, 22),
            mark("LayerTreeHostRenderingUpdate", 120020, 120022),
        ]

        stdout = self._draw(marks, resolution=10.0, max_cells=8, tile_lane=False)

        lines = [line for line in stdout.split("\n") if "▕" in line]
        columns = {line.index("▕") for line in lines}
        self.assertEqual(len(columns), 1)
        self.assertIn("120000.00 ms", stdout)
        labels, ticks = self._ruler(
            marks, resolution=10.0, max_cells=8, tile_lane=False
        )
        self.assertEqual(ticks.index("┼"), columns.pop() + 1)
        self.assertEqual(labels.index("0"), ticks.index("┼"))


if __name__ == "__main__":
    unittest.main()
