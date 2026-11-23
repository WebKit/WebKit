# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Chart geometry: what the SVG in the template is drawn from."""

from __future__ import annotations

import datetime
import unittest

from ews_dashboard import config
from ews_dashboard.analysis import false_positive, trend
from ews_dashboard.web import chart

FIRST_DAY = datetime.date(2026, 8, 1)


def _counts(blamed: int, clean: int) -> false_positive.Counts:
    counts = false_positive.Counts()
    for _ in range(blamed):
        counts.record(false_positive.Classification(false_positive.FALSE_RED, 1, 1, 0, 0))
    for _ in range(clean):
        counts.record(false_positive.Classification(false_positive.CLEAN, 1, 0, 1, 0))
    return counts


def _points(daily_pcts: list) -> list:
    """One point per value, oldest first. None means a day with no classifiable builds."""
    points = []
    for offset, pct in enumerate(daily_pcts):
        blamed = 0 if pct is None else int(pct / 10)
        clean = 0 if pct is None else 10 - blamed
        points.append(trend.Point(FIRST_DAY + datetime.timedelta(days=offset),
                                  _counts(blamed, clean), pct))
    return points


class TestChart(unittest.TestCase):
    def test_an_empty_series_draws_nothing_but_still_has_a_scale(self) -> None:
        drawn = chart.of_trend([])
        self.assertTrue(drawn.empty)
        self.assertEqual(drawn.rolling_segments, ())
        self.assertEqual(drawn.y_max, chart.SMALLEST_Y_MAX_PCT)

    def test_a_low_rate_keeps_the_smallest_scale_so_noise_does_not_look_like_a_crisis(self) -> None:
        self.assertEqual(chart.of_trend(_points([1.0, 2.0])).y_max, chart.SMALLEST_Y_MAX_PCT)

    def test_the_scale_rounds_up_past_the_highest_value(self) -> None:
        self.assertEqual(chart.of_trend(_points([42.0])).y_max, 50)

    def test_zero_percent_sits_on_the_bottom_of_the_plot(self) -> None:
        dot = chart.of_trend(_points([0.0])).dots[0]
        self.assertEqual(dot.y, chart.PLOT_BOTTOM)

    def test_the_scale_maximum_sits_on_the_top_of_the_plot(self) -> None:
        drawn = chart.of_trend(_points([50.0]))
        self.assertEqual(drawn.dots[0].y, chart.PLOT_TOP)

    def test_a_day_with_no_classifiable_builds_gets_no_dot_rather_than_a_zero(self) -> None:
        drawn = chart.of_trend(_points([10.0, None, 20.0]))
        self.assertEqual(len(drawn.dots), 2)

    def test_every_dot_names_the_counts_behind_its_percentage(self) -> None:
        title = chart.of_trend(_points([20.0])).dots[0].title
        self.assertIn('2 of 10 builds', title)
        self.assertIn(FIRST_DAY.isoformat(), title)

    def test_the_rolling_line_breaks_where_the_data_does(self) -> None:
        drawn = chart.of_trend(_points([10.0, 20.0, None, 30.0, 40.0]))
        missing_day_x = chart.PLOT_LEFT + chart.PLOT_WIDTH * 2.5 / 5
        self.assertEqual(len(drawn.rolling_segments), 2)
        for segment in drawn.rolling_segments:
            self.assertFalse(segment.x1 < missing_day_x < segment.x2)

    def test_the_rolling_line_is_one_stroke_where_the_data_is_continuous(self) -> None:
        segments = chart.of_trend(_points([10.0, 20.0, 30.0])).rolling_segments
        self.assertEqual(len(segments), 2)
        self.assertEqual(segments[0].x2, segments[1].x1)
        self.assertEqual(segments[0].y2, segments[1].y1)

    def test_dots_stay_inside_the_plot_horizontally(self) -> None:
        drawn = chart.of_trend(_points([10.0] * 30))
        self.assertGreater(drawn.dots[0].x, chart.PLOT_LEFT)
        self.assertLess(drawn.dots[-1].x, chart.PLOT_RIGHT)

    def test_a_deployment_is_marked_where_it_happened_in_the_series(self) -> None:
        deployment = config.Deployment(
            at=datetime.datetime(2026, 8, 3, 12, tzinfo=datetime.timezone.utc),
            label='read', detail='a deployment',
        )
        drawn = chart.of_trend(_points([10.0] * 5), [deployment])
        middle = chart.PLOT_LEFT + chart.PLOT_WIDTH / 2
        self.assertAlmostEqual(drawn.deployments[0].x, middle, places=0)
        self.assertIn('a deployment', drawn.deployments[0].title)

    def test_a_single_day_series_is_drawn_rather_than_dividing_by_zero(self) -> None:
        drawn = chart.of_trend(_points([10.0]))
        self.assertEqual(len(drawn.dots), 1)
        self.assertAlmostEqual(drawn.dots[0].x, chart.PLOT_LEFT + chart.PLOT_WIDTH / 2, places=0)

    def test_the_last_day_is_always_labelled(self) -> None:
        drawn = chart.of_trend(_points([10.0] * 30))
        self.assertEqual(drawn.day_labels[-1].x, drawn.dots[-1].x)
