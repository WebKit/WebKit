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

"""Staleness, which the page must show rather than hide behind confident-looking numbers."""

from __future__ import annotations

import time
from typing import Optional

from ews_dashboard.analysis import freshness
from tests import fixtures

NOW = int(time.time())
LONG_AGO = NOW - 30 * 86400


class FreshnessTest(fixtures.DatabaseTest):
    def record_refresh(self, finished_at: Optional[int], started_at: Optional[int] = None,
                       error: Optional[str] = None) -> None:
        with self.connection:
            self.connection.execute(
                'INSERT INTO refresh_runs (started_at, finished_at, error) VALUES (?, ?, ?)',
                (started_at if started_at is not None else NOW - 90000, finished_at, error),
            )

    def record_classification(self, build_id: int, classified_at: int) -> None:
        with self.connection:
            self.connection.execute(
                'INSERT INTO build_classifications (build_id, threshold_pct, bucket, surfaced_total, '
                'surfaced_pre_existing, surfaced_real, surfaced_undetermined, classified_at) '
                'VALUES (?, 80, ?, 1, 1, 0, 0, ?)',
                (build_id, 'FALSE_RED', classified_at),
            )

    def current(self) -> freshness.Freshness:
        return freshness.current(self.connection)


class TestRefreshAge(FreshnessTest):
    def test_a_database_that_was_never_refreshed_is_stale_rather_than_new(self) -> None:
        current = self.current()
        self.assertIsNone(current.refreshed_at)
        self.assertIsNone(current.refresh_age_seconds)
        self.assertTrue(current.stale)

    def test_a_recent_refresh_is_fresh(self) -> None:
        self.record_refresh(NOW - 300)
        current = self.current()
        self.assertLess(current.refresh_age_seconds, 600)
        self.assertFalse(current.stale)

    def test_a_refresh_older_than_the_limit_is_stale(self) -> None:
        self.record_refresh(NOW - freshness.REFRESH_STALE_AFTER_SECONDS - 60)
        self.assertTrue(self.current().stale)

    def test_the_newest_refresh_is_the_one_reported(self) -> None:
        self.record_refresh(LONG_AGO, started_at=LONG_AGO - 60)
        self.record_refresh(NOW - 300)
        self.assertEqual(self.current().refreshed_at, NOW - 300)

    def test_a_refresh_still_running_does_not_count_as_one_that_finished(self) -> None:
        self.record_refresh(NOW - 300)
        self.record_refresh(None, started_at=NOW - 10)
        current = self.current()
        self.assertEqual(current.refreshed_at, NOW - 300)
        self.assertFalse(current.stale)


class TestClassificationLag(FreshnessTest):
    def setUp(self) -> None:
        super().setUp()
        self.build_id = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])

    def test_classifications_that_kept_up_with_the_refresh_are_fresh(self) -> None:
        self.record_refresh(NOW - 300)
        self.record_classification(self.build_id, NOW - 600)
        current = self.current()
        self.assertEqual(current.classification_lag_seconds, 300)
        self.assertFalse(current.stale)

    def test_classifications_left_far_behind_a_fresh_refresh_are_stale(self) -> None:
        self.record_refresh(NOW - 300)
        self.record_classification(self.build_id, LONG_AGO)
        current = self.current()
        self.assertGreater(current.classification_lag_seconds,
                           freshness.CLASSIFICATION_LAG_STALE_AFTER_SECONDS)
        self.assertTrue(current.stale)

    def test_a_classification_newer_than_the_refresh_is_no_lag_rather_than_a_negative_one(self) -> None:
        self.record_refresh(NOW - 600)
        self.record_classification(self.build_id, NOW - 300)
        self.assertEqual(self.current().classification_lag_seconds, 0)

    def test_nothing_classified_yet_is_not_a_lag_the_page_can_measure(self) -> None:
        self.record_refresh(NOW - 300)
        current = self.current()
        self.assertIsNone(current.classified_at)
        self.assertIsNone(current.classification_lag_seconds)
        self.assertFalse(current.stale)


class TestFailedRefresh(FreshnessTest):
    def test_a_refresh_that_died_is_stale_even_though_an_earlier_one_finished_recently(self) -> None:
        self.record_refresh(NOW - 300)
        self.record_refresh(None, started_at=NOW - 120, error='URLError: offline')
        current = self.current()
        self.assertEqual(current.reason, freshness.REFRESH_FAILED)
        self.assertEqual(current.failure, 'URLError: offline')
        self.assertTrue(current.stale)

    def test_a_later_successful_refresh_clears_an_earlier_failure(self) -> None:
        self.record_refresh(None, started_at=NOW - 900, error='URLError: offline')
        self.record_refresh(NOW - 300, started_at=NOW - 600)
        current = self.current()
        self.assertIsNone(current.failure)
        self.assertFalse(current.stale)


class TestStalenessReason(FreshnessTest):
    def test_an_overdue_refresh_is_not_reported_as_a_classification_problem(self) -> None:
        self.record_refresh(NOW - freshness.REFRESH_STALE_AFTER_SECONDS - 60)
        self.assertEqual(self.current().reason, freshness.REFRESH_OVERDUE)

    def test_classification_falling_behind_is_named_as_that_and_not_as_the_interval(self) -> None:
        build_id = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        self.record_refresh(NOW - 300)
        self.record_classification(build_id, LONG_AGO)
        self.assertEqual(self.current().reason, freshness.CLASSIFICATION_BEHIND)

    def test_a_database_nothing_has_refreshed_says_so(self) -> None:
        self.assertEqual(self.current().reason, freshness.NEVER_REFRESHED)

    def test_a_page_with_nothing_wrong_has_no_reason_to_show(self) -> None:
        self.record_refresh(NOW - 300)
        self.assertIsNone(self.current().reason)


class TestNewestBuild(FreshnessTest):
    def test_the_newest_ingested_build_is_reported_so_a_quiet_queue_is_visible(self) -> None:
        self.store_build(1, started_at=fixtures.DEFAULT_BUILD_TIME)
        self.store_build(2, started_at=fixtures.DEFAULT_BUILD_TIME + 3600)
        self.assertEqual(self.current().newest_build_at, fixtures.DEFAULT_BUILD_TIME + 3600)

    def test_an_empty_database_has_no_newest_build(self) -> None:
        self.assertIsNone(self.current().newest_build_at)
