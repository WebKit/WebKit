"""The helpers webkitsysprof.utils holds, which every command reads marks with."""

import unittest

from .helpers import SysprofTestCase, approx, mark, sysprof_data

from webkitsysprof.utils import (
    MarkIndex,
    UsageError,
    marks_in_time_order,
    merged_spans,
    msec_to_nsec,
    parse_timespan_argument,
    percentiles,
    sysprof_data_with_marks_by_name,
    intervals_between_marks,
    median_vblank_interval,
    display_refreshes,
)


def _window(begin_msec, end_msec):
    return msec_to_nsec(begin_msec), msec_to_nsec(end_msec)


class UtilsTest(SysprofTestCase):
    def test_the_marks_overlapping_a_window_are_the_ones_running_in_it(self):
        index = MarkIndex(
            [
                mark("Enclosing", 0, 100),
                mark("Early", 10, 12),
                mark("Inside", 40, 60),
                mark("Late", 90, 95),
            ]
        )

        # In begin order, and the long mark that began well before the window is
        # found even though the walk back to it passes marks that end before it.
        self.assertEqual(
            [found["name"] for found in index.marks_overlapping(*_window(30, 50))],
            ["Enclosing", "Inside"],
        )

    def test_a_mark_touching_a_window_at_its_edge_does_not_overlap_it(self):
        # Half open at both ends, like every other window these are read through, so
        # a mark of the neighbouring cycle stays out of this one.
        index = MarkIndex([mark("Before", 0, 10), mark("After", 20, 30)])

        self.assertEqual(index.marks_overlapping(*_window(10, 20)), [])

    def test_an_index_of_no_marks_has_none_overlapping_anything(self):
        self.assertEqual(MarkIndex([]).marks_overlapping(*_window(0, 10)), [])

    def test_spans_are_merged_where_they_overlap_or_touch(self):
        self.assertEqual(
            merged_spans([(0, 5), (3, 8), (8, 9), (20, 25)]), [(0, 9), (20, 25)]
        )
        # An empty span is no span at all, rather than one of no length in between.
        self.assertEqual(merged_spans([(5, 5), (10, 12)]), [(10, 12)])
        self.assertEqual(merged_spans([]), [])

    def test_a_timespan_bound_is_digits_and_nothing_else(self):
        # int() would take every one of these and analyze a window nobody asked for.
        for timespan in [
            "  5",
            "+5",
            "0-1_000",
            "5\n",
            "500-100",
            # Of no length, so it encloses nothing between its bounds.
            "0-0",
            "500-500",
            "abc",
            "0-1e3",
            "1-2-3",
            "",
        ]:
            with self.subTest(timespan=timespan), self.assertRaises(UsageError):
                parse_timespan_argument(timespan)

    def test_percentiles_outside_the_range_of_the_quantiles_are_rejected(self):
        # Percentile 0 used to index the list from the back and report P99 as P0.
        for percentile in [0, 100, -1]:
            with self.subTest(percentile=percentile), self.assertRaises(ValueError):
                percentiles([1.0, 2.0, 3.0], [percentile])

    def test_a_bare_bound_is_the_begin_of_the_timespan(self):
        self.assertEqual(parse_timespan_argument("500"), (msec_to_nsec(500), None))

    def test_vblank_intervals_do_not_depend_on_the_order_of_the_marks(self):
        data = sysprof_data(
            [
                mark("DisplayLinkUpdate", 32, 33),
                mark("DisplayLinkUpdate", 0, 1),
                mark("DisplayLinkUpdate", 16, 17),
            ]
        )

        self.assertEqual(
            intervals_between_marks(marks_in_time_order(data, "DisplayLinkUpdate")),
            [
                approx(16.0),
                approx(16.0),
            ],
        )
        self.assertEqual(
            median_vblank_interval(display_refreshes(data)),
            approx(16.0),
        )

    def test_reshaping_the_same_parsed_data_twice_yields_the_same_result(self):
        parsed_data = {
            "document": {"timespan": [0, msec_to_nsec(1000)]},
            "marks": [mark("LayerTreeHostRenderingUpdate", 0, 5)],
        }

        first = sysprof_data_with_marks_by_name(parsed_data)
        second = sysprof_data_with_marks_by_name(parsed_data)
        self.assertEqual(first["document"]["timespan"], second["document"]["timespan"])
        self.assertEqual(parsed_data["document"]["timespan"], [0, msec_to_nsec(1000)])


if __name__ == "__main__":
    unittest.main()
