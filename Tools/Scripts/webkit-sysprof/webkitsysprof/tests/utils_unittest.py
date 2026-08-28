"""The helpers webkitsysprof.utils holds, which every command reads marks with."""

import unittest

from .helpers import SysprofTestCase, approx, mark, sysprof_data

from webkitsysprof.utils import (
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


class UtilsTest(SysprofTestCase):
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
