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

"""Daily buckets are UTC, and the rolling value sums counts rather than averaging percentages."""

from __future__ import annotations

import datetime

from ews_dashboard import config
from ews_dashboard.analysis import false_positive, trend
from tests import fixtures

LAST_DAY = datetime.date(2026, 8, 20)
RELIABLE = 99.5
UNRELIABLE = 40.0


def _noon(day: datetime.date) -> int:
    return trend.day_bounds(day)[0] + 43200


class TestDaily(fixtures.DatabaseTest):
    def _points(self, days: int = 3, rolling: int = trend.ROLLING_DAYS,
                builders: tuple = ()) -> list:
        history = fixtures.StubHistory({'fast/pre.html': UNRELIABLE, 'fast/real.html': RELIABLE})
        return trend.daily(
            self.connection, false_positive.live_classifier(self.connection, history),
            LAST_DAY, days=days, rolling=rolling, builders=builders,
        )

    def _store_blamed_build(self, number: int, day: datetime.date, blamed: bool = True,
                            builder: str = fixtures.LAYOUT_BUILDER, builder_id: int = 7) -> None:
        test_name = 'fast/pre.html' if blamed else 'fast/real.html'
        self.store_build(number, first=[test_name], second=[test_name], clean=[],
                         started_at=_noon(day), builder=builder, builder_id=builder_id)

    def test_one_point_per_day_oldest_first(self) -> None:
        points = self._points(days=3)
        self.assertEqual([point.day for point in points],
                         [LAST_DAY - datetime.timedelta(days=2),
                          LAST_DAY - datetime.timedelta(days=1), LAST_DAY])

    def test_a_day_with_no_builds_has_no_rate_rather_than_a_zero(self) -> None:
        self.assertIsNone(self._points()[0].daily_pct)

    def test_a_days_rate_covers_only_that_day(self) -> None:
        self._store_blamed_build(1, LAST_DAY)
        self._store_blamed_build(2, LAST_DAY - datetime.timedelta(days=1), blamed=False)
        points = self._points(days=2)
        self.assertEqual(points[0].daily_pct, 0.0)
        self.assertEqual(points[1].daily_pct, 100.0)

    def test_a_build_late_in_a_utc_day_belongs_to_that_day(self) -> None:
        last_second = trend.day_bounds(LAST_DAY)[1] - 1
        self.store_build(1, first=['fast/pre.html'], second=['fast/pre.html'], clean=[],
                         started_at=last_second)
        self.assertEqual(self._points(days=1)[0].daily_pct, 100.0)

    def test_the_rolling_value_weights_a_busy_day_more_than_a_quiet_one(self) -> None:
        quiet_day = LAST_DAY - datetime.timedelta(days=1)
        self._store_blamed_build(1, quiet_day)
        for number in range(2, 11):
            self._store_blamed_build(number, LAST_DAY, blamed=False)
        points = self._points(days=2)
        self.assertEqual(points[0].daily_pct, 100.0)
        self.assertEqual(points[1].daily_pct, 0.0)
        # 1 blamed build out of 10 classifiable across the window, not the mean of 100% and 0%.
        self.assertEqual(points[1].rolling_pct, 10.0)

    def test_the_rolling_value_reaches_back_beyond_the_first_returned_day(self) -> None:
        self._store_blamed_build(1, LAST_DAY - datetime.timedelta(days=3))
        points = self._points(days=2)
        self.assertIsNone(points[0].daily_pct)
        self.assertEqual(points[0].rolling_pct, 100.0)

    def test_a_shorter_rolling_average_forgets_a_day_a_longer_one_still_covers(self) -> None:
        self._store_blamed_build(1, LAST_DAY - datetime.timedelta(days=4))
        self.assertEqual(self._points(days=1, rolling=7)[0].rolling_pct, 100.0)
        self.assertIsNone(self._points(days=1, rolling=3)[0].rolling_pct)

    def test_the_points_count_only_the_queue_they_are_narrowed_to(self) -> None:
        self._store_blamed_build(1, LAST_DAY)
        self._store_blamed_build(2, LAST_DAY, blamed=False,
                                 builder=fixtures.GTK_BUILDER, builder_id=11)
        self.assertEqual(self._points(days=1)[0].daily_pct, 50.0)
        self.assertEqual(self._points(days=1, builders=(fixtures.LAYOUT_BUILDER,))[0].daily_pct, 100.0)
        self.assertEqual(self._points(days=1, builders=(fixtures.GTK_BUILDER,))[0].daily_pct, 0.0)


class TestDeploymentsWithin(fixtures.DatabaseTest):
    def _points(self, days: list) -> list:
        return [trend.Point(day, false_positive.Counts(), None) for day in days]

    def test_a_deployment_inside_the_range_is_reported(self) -> None:
        deployment = config.DEPLOYMENTS[0]
        day = deployment.at.date()
        found = trend.deployments_within(self._points([day - datetime.timedelta(days=1), day]))
        self.assertEqual(found, [deployment])

    def test_a_deployment_outside_the_range_is_not(self) -> None:
        day = config.DEPLOYMENTS[0].at.date() - datetime.timedelta(days=30)
        self.assertEqual(trend.deployments_within(self._points([day])), [])

    def test_an_empty_series_has_no_deployments_to_mark(self) -> None:
        self.assertEqual(trend.deployments_within([]), [])
