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

"""Which builds count as false red, and what the rate is a rate over."""

from __future__ import annotations

import time
from typing import Optional
from unittest import mock

from ews_dashboard import config, results
from ews_dashboard.analysis import false_positive
from ews_dashboard.web import formatting
from tests import fixtures

WINDOW = (fixtures.DEFAULT_BUILD_TIME - 86400, fixtures.DEFAULT_BUILD_TIME + 86400)

RELIABLE = 99.5
UNRELIABLE = 40.0
AT_THRESHOLD = float(config.PRE_EXISTING_THRESHOLD_PCT)


def summary(pass_pct: float) -> dict:
    """The nine percentages the endpoint returns, with whatever is left over spent on failures."""
    return {'pass': pass_pct, 'fail': 100.0 - pass_pct, 'timeout': 0.0, 'crash': 0.0, 'image': 0.0,
            'audio': 0.0, 'text': 0.0, 'error': 0.0, 'warning': 0.0}


class TestSurfacedTests(fixtures.DatabaseTest):
    def test_only_failures_in_both_runs_and_not_on_a_clean_tree_reach_the_author(self) -> None:
        build_id = self.store_build(
            1,
            first=['fast/both.html', 'fast/first-only.html', 'fast/pre.html'],
            second=['fast/both.html', 'fast/second-only.html', 'fast/pre.html'],
            clean=['fast/pre.html'],
        )
        self.assertEqual(false_positive.surfaced_tests(self.stored_build(build_id)),
                         ['fast/both.html'])

    def test_a_build_with_no_recorded_lists_surfaced_nothing(self) -> None:
        build_id = self.store_build(1)
        self.assertEqual(false_positive.surfaced_tests(self.stored_build(build_id)), [])


class TestClassification(fixtures.DatabaseTest):
    def _classify(self, history: object, build_id: int) -> false_positive.Classification:
        return false_positive.classify(self.connection, history, self.stored_build(build_id))

    def test_every_surfaced_test_reliable_on_main_is_a_clean_build(self) -> None:
        build_id = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        classification = self._classify(fixtures.StubHistory({'fast/a.html': RELIABLE}), build_id)
        self.assertEqual(classification.bucket, false_positive.CLEAN)
        self.assertEqual(classification.surfaced_real, 1)

    def test_every_surfaced_test_already_failing_on_main_is_a_false_red(self) -> None:
        build_id = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        classification = self._classify(fixtures.StubHistory({'fast/a.html': UNRELIABLE}), build_id)
        self.assertEqual(classification.bucket, false_positive.FALSE_RED)
        self.assertEqual(classification.surfaced_pre_existing, 1)

    def test_a_mix_is_a_partial_false_positive(self) -> None:
        build_id = self.store_build(1, first=['fast/a.html', 'fast/b.html'],
                                    second=['fast/a.html', 'fast/b.html'], clean=[])
        classification = self._classify(
            fixtures.StubHistory({'fast/a.html': RELIABLE, 'fast/b.html': UNRELIABLE}), build_id,
        )
        self.assertEqual(classification.bucket, false_positive.PARTIAL_FP)
        self.assertEqual((classification.surfaced_real, classification.surfaced_pre_existing), (1, 1))

    def test_a_test_with_no_recorded_history_is_undetermined_not_real(self) -> None:
        build_id = self.store_build(1, first=['fast/new.html'], second=['fast/new.html'], clean=[])
        classification = self._classify(fixtures.StubHistory({}), build_id)
        self.assertEqual(classification.bucket, false_positive.UNDETERMINED)
        self.assertEqual(classification.surfaced_undetermined, 1)

    def test_an_outage_is_undetermined_rather_than_an_answer(self) -> None:
        build_id = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        classification = self._classify(
            fixtures.StubHistory({'fast/a.html': RELIABLE}, unavailable={'fast/a.html'}), build_id,
        )
        self.assertEqual(classification.bucket, false_positive.UNDETERMINED)

    def test_the_threshold_includes_its_own_value(self) -> None:
        at_threshold = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        just_above = self.store_build(2, first=['fast/b.html'], second=['fast/b.html'], clean=[])
        history = fixtures.StubHistory({
            'fast/a.html': float(config.PRE_EXISTING_THRESHOLD_PCT),
            'fast/b.html': config.PRE_EXISTING_THRESHOLD_PCT + 0.1,
        })
        self.assertEqual(self._classify(history, at_threshold).bucket, false_positive.FALSE_RED)
        self.assertEqual(self._classify(history, just_above).bucket, false_positive.CLEAN)

    def test_a_build_that_surfaced_nothing_has_no_bucket_at_all(self) -> None:
        build_id = self.store_build(1, first=['fast/a.html'], second=[], clean=[])
        self.assertIsNone(self._classify(fixtures.UnreachableHistory(), build_id).bucket)

    def test_a_crash_storm_is_undetermined_without_a_single_lookup(self) -> None:
        build_id = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[],
                                    exceeded_failure_limit=True)
        classification = self._classify(fixtures.UnreachableHistory(), build_id)
        self.assertEqual(classification.bucket, false_positive.UNDETERMINED)

    def test_more_surfaced_tests_than_a_change_plausibly_breaks_is_undetermined(self) -> None:
        many = [f'fast/{index}.html' for index in range(config.MAX_CLASSIFIABLE_SURFACED_TESTS + 1)]
        build_id = self.store_build(1, first=many, second=many, clean=[])
        classification = self._classify(fixtures.UnreachableHistory(), build_id)
        self.assertEqual(classification.bucket, false_positive.UNDETERMINED)
        self.assertEqual(classification.surfaced_undetermined, len(many))

    def test_a_build_with_no_base_identifier_is_undetermined_without_a_single_lookup(self) -> None:
        build_id = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[],
                                    identifier=None)
        classification = self._classify(fixtures.UnreachableHistory(), build_id)
        self.assertEqual(classification.bucket, false_positive.UNDETERMINED)
        self.assertEqual(classification.surfaced_undetermined, 1)

    def test_the_lookup_asks_about_the_identifier_and_never_the_pull_requests_own_head(self) -> None:
        build_id = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        history = fixtures.StubHistory({'fast/a.html': RELIABLE})
        self._classify(history, build_id)
        self.assertEqual([query.commit_ref for query in history.queries], [fixtures.IDENTIFIER])

    def test_a_classification_is_made_once(self) -> None:
        build_id = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        history = fixtures.StubHistory({'fast/a.html': RELIABLE})
        self._classify(history, build_id)
        self._classify(fixtures.UnreachableHistory(), build_id)
        self.assertEqual(history.asked, ['fast/a.html'])

    def test_an_undetermined_classification_is_retried_once_it_has_aged(self) -> None:
        build_id = self.store_build(1, first=['fast/new.html'], second=['fast/new.html'], clean=[])
        self._classify(fixtures.StubHistory({}), build_id)
        self._age_classifications(false_positive.UNDETERMINED_TTL_SECONDS + 60)
        history = fixtures.StubHistory({'fast/new.html': RELIABLE})
        self.assertEqual(self._classify(history, build_id).bucket, false_positive.CLEAN)
        self.assertEqual(history.asked, ['fast/new.html'])

    def test_a_settled_classification_is_never_retried_however_old(self) -> None:
        build_id = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        self._classify(fixtures.StubHistory({'fast/a.html': RELIABLE}), build_id)
        self._age_classifications(100 * false_positive.UNDETERMINED_TTL_SECONDS)
        self.assertEqual(self._classify(fixtures.UnreachableHistory(), build_id).bucket,
                         false_positive.CLEAN)

    def _age_classifications(self, seconds: int) -> None:
        with self.connection:
            self.connection.execute('UPDATE build_classifications SET classified_at = ?',
                                    (int(time.time()) - seconds,))


class TestCounts(fixtures.DatabaseTest):
    def test_a_rate_over_no_classifiable_builds_is_unknown_rather_than_zero(self) -> None:
        counts = false_positive.Counts()
        self.assertIsNone(counts.author_fp_rate_pct)
        self.assertIsNone(counts.false_red_rate_pct)
        self.assertIsNone(counts.blame_noise_rate_pct)

    def test_builds_with_no_bucket_stay_out_of_the_denominator(self) -> None:
        counts = false_positive.Counts()
        counts.record(false_positive.Classification(false_positive.CLEAN, 1, 0, 1, 0))
        counts.record(false_positive.Classification(false_positive.FALSE_RED, 1, 1, 0, 0))
        counts.record(false_positive.Classification(None))
        counts.record(None)
        self.assertEqual(counts.classifiable, 2)
        self.assertEqual(counts.author_fp_rate_pct, 50.0)
        self.assertEqual((counts.no_surfaced, counts.unclassified), (1, 1))

    def test_undetermined_builds_are_counted_but_not_scored(self) -> None:
        counts = false_positive.Counts()
        counts.record(false_positive.Classification(false_positive.UNDETERMINED, 3, 0, 0, 3))
        self.assertEqual(counts.undetermined, 1)
        self.assertEqual(counts.classifiable, 0)
        self.assertIsNone(counts.author_fp_rate_pct)


class TestRate(fixtures.DatabaseTest):
    def setUp(self) -> None:
        super().setUp()
        self.store_build(1, first=['fast/pre.html'], second=['fast/pre.html'], clean=[])
        self.store_build(2, first=['fast/real.html'], second=['fast/real.html'], clean=[],
                         builder=fixtures.API_BUILDER, builder_id=9)
        self.history = fixtures.StubHistory({'fast/pre.html': UNRELIABLE, 'fast/real.html': RELIABLE})

    def _rate(self, **filters: object) -> false_positive.Counts:
        classifier = false_positive.live_classifier(self.connection, self.history)
        return false_positive.rate(self.connection, classifier, *WINDOW, **filters)

    def test_every_failing_build_in_the_window_is_scored(self) -> None:
        counts = self._rate()
        self.assertEqual((counts.false_red, counts.clean), (1, 1))
        self.assertEqual(counts.author_fp_rate_pct, 50.0)

    def test_a_suite_filter_narrows_the_rate(self) -> None:
        self.assertEqual(self._rate(suite='api-tests').clean, 1)
        self.assertEqual(self._rate(suite='api-tests').false_red, 0)

    def test_a_builder_filter_narrows_the_rate(self) -> None:
        counts = self._rate(builders=(fixtures.LAYOUT_BUILDER,))
        self.assertEqual((counts.false_red, counts.clean), (1, 0))

    def test_builds_outside_the_window_are_not_scored(self) -> None:
        classifier = false_positive.live_classifier(self.connection, self.history)
        counts = false_positive.rate(self.connection, classifier,
                                     WINDOW[1], WINDOW[1] + 86400)
        self.assertEqual(counts.classifiable, 0)

    def test_a_successful_build_is_not_scored(self) -> None:
        self.store_build(3, first=[], second=[], clean=[], results_code=0)
        self.assertEqual(len(false_positive.failing_builds(self.connection, *WINDOW)), 2)


class TestClassifierSeparation(fixtures.DatabaseTest):
    def setUp(self) -> None:
        super().setUp()
        self.build_id = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])

    def test_the_cached_classifier_reports_an_unclassified_build_rather_than_classifying_it(self) -> None:
        counts = false_positive.rate(
            self.connection, false_positive.cached_classifier(self.connection), *WINDOW,
        )
        self.assertEqual(counts.unclassified, 1)
        self.assertEqual(counts.classifiable, 0)
        self.assertIsNone(counts.author_fp_rate_pct)

    def test_the_cached_classifier_reads_what_the_live_one_wrote(self) -> None:
        false_positive.rate(
            self.connection,
            false_positive.live_classifier(self.connection, fixtures.StubHistory({'fast/a.html': RELIABLE})),
            *WINDOW,
        )
        counts = false_positive.rate(
            self.connection, false_positive.cached_classifier(self.connection), *WINDOW,
        )
        self.assertEqual((counts.clean, counts.unclassified), (1, 0))


class TestPendingQueries(fixtures.DatabaseTest):
    def _pending(self) -> list:
        builds = false_positive.failing_builds(self.connection, *WINDOW)
        return list(false_positive.pending_queries(self.connection, builds))

    def test_one_query_per_surfaced_test(self) -> None:
        self.store_build(1, first=['fast/a.html', 'fast/b.html'],
                         second=['fast/a.html', 'fast/b.html'], clean=['fast/b.html'])
        self.assertEqual([query.test_name for query in self._pending()], ['fast/a.html'])

    def test_the_query_asks_about_the_commit_the_change_was_rebased_onto(self) -> None:
        self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        self.assertEqual(self._pending()[0].commit_ref, fixtures.IDENTIFIER)

    def test_an_already_classified_build_needs_nothing(self) -> None:
        build_id = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        false_positive.classify(self.connection, fixtures.StubHistory({'fast/a.html': RELIABLE}),
                                self.stored_build(build_id))
        self.assertEqual(self._pending(), [])

    def test_a_crash_storm_needs_nothing(self) -> None:
        self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[],
                         exceeded_failure_limit=True)
        self.assertEqual(self._pending(), [])


class TestUndeterminedReason(fixtures.DatabaseTest):
    def _reason(self, build_id: int) -> Optional[str]:
        return false_positive.undetermined_reason(self.stored_build(build_id))

    def test_a_build_over_the_bots_failure_limit_reads_as_truncated_lists(self) -> None:
        build_id = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[],
                                    exceeded_failure_limit=True)
        self.assertEqual(self._reason(build_id), false_positive.TRUNCATED_LISTS)
        self.assertIn(self._reason(build_id), false_positive.REASON_DESCRIPTIONS)

    def test_a_build_that_recorded_no_identifier_reads_as_having_no_base_commit(self) -> None:
        build_id = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[],
                                    identifier=None)
        self.assertEqual(self._reason(build_id), false_positive.NO_BASE_COMMIT)
        self.assertIn(self._reason(build_id), false_positive.REASON_DESCRIPTIONS)

    def test_more_surfaced_tests_than_a_change_plausibly_breaks_reads_as_too_many_surfaced(self) -> None:
        many = [f'fast/{index}.html' for index in range(config.MAX_CLASSIFIABLE_SURFACED_TESTS + 1)]
        build_id = self.store_build(1, first=many, second=many, clean=[])
        self.assertEqual(self._reason(build_id), false_positive.TOO_MANY_SURFACED)
        self.assertIn(self._reason(build_id), false_positive.REASON_DESCRIPTIONS)

    def test_an_ordinary_build_has_no_reason_its_history_cannot_be_believed(self) -> None:
        build_id = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        self.assertIsNone(self._reason(build_id))


class TestBucketDescriptions(fixtures.DatabaseTest):
    """The gloss the legend reads for every state the builds pane can show a build in."""

    def test_every_state_choice_has_a_description(self) -> None:
        for choice in formatting.STATE_CHOICES:
            self.assertIn(choice, false_positive.BUCKET_DESCRIPTIONS, choice)


class TestExplain(fixtures.DatabaseTest):
    """The same partition `classify` made, per test, out of the cache a refresh filled."""

    def _explain(self, build_id: int) -> list:
        return false_positive.explain(self.connection, self.stored_build(build_id))

    def _cache(self, build_id: int, test_name: str, outcomes: Optional[dict]) -> None:
        build_row = self.stored_build(build_id)
        self.cache_answer(
            results.Query(test_name, results.Configuration.of_build(build_row),
                          false_positive.base_commit_of(build_row) or ''),
            outcomes,
        )

    def test_one_entry_per_surfaced_test_in_the_order_surfaced_tests_returns_them(self) -> None:
        build_id = self.store_build(1, first=['fast/c.html', 'fast/a.html', 'fast/b.html'],
                                    second=['fast/c.html', 'fast/a.html', 'fast/b.html'], clean=[])
        self.assertEqual([entry.name for entry in self._explain(build_id)],
                         false_positive.surfaced_tests(self.stored_build(build_id)))

    def test_a_test_main_already_fails_at_the_threshold_is_pre_existing(self) -> None:
        build_id = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        self._cache(build_id, 'fast/a.html', summary(AT_THRESHOLD))
        entry = self._explain(build_id)[0]
        self.assertEqual((entry.verdict, entry.pass_rate),
                         (false_positive.PRE_EXISTING, AT_THRESHOLD))

    def test_a_test_main_passes_above_the_threshold_is_real(self) -> None:
        build_id = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        self._cache(build_id, 'fast/a.html', summary(RELIABLE))
        entry = self._explain(build_id)[0]
        self.assertEqual((entry.verdict, entry.pass_rate), (false_positive.REAL, RELIABLE))

    def test_a_cached_row_saying_upstream_has_no_history_reads_as_no_history(self) -> None:
        build_id = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        self._cache(build_id, 'fast/a.html', None)
        entry = self._explain(build_id)[0]
        self.assertEqual((entry.verdict, entry.pass_rate), (false_positive.NO_HISTORY, None))

    def test_a_test_nothing_has_looked_up_reads_as_unqueried_rather_than_as_having_no_history(self) -> None:
        """The first sends a reader to the refresh, the second to results.webkit.org."""
        build_id = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        self.assertEqual(self._explain(build_id)[0].verdict, false_positive.UNQUERIED)

    def test_an_unanswerable_build_reports_every_test_unqueried_without_a_single_cache_lookup(self) -> None:
        build_id = self.store_build(1, first=['fast/a.html', 'fast/b.html'],
                                    second=['fast/a.html', 'fast/b.html'], clean=[],
                                    exceeded_failure_limit=True)
        self._cache(build_id, 'fast/a.html', summary(RELIABLE))
        with mock.patch('ews_dashboard.analysis.false_positive.results.cached_answer') as cached_answer:
            self.assertEqual([entry.verdict for entry in self._explain(build_id)],
                             [false_positive.UNQUERIED, false_positive.UNQUERIED])
            self.assertEqual(cached_answer.call_count, 0)

    def test_the_per_test_verdicts_add_up_to_the_counts_classify_recorded_for_the_same_build(self) -> None:
        """The counts a page shows and the tests it lists come from two passes over the same cache,
        so a drift between them reads as a page contradicting itself."""
        surfaced = ['fast/none.html', 'fast/pre.html', 'fast/real.html', 'fast/unknown.html']
        build_id = self.store_build(1, first=surfaced, second=surfaced, clean=[])
        self._cache(build_id, 'fast/pre.html', summary(AT_THRESHOLD))
        self._cache(build_id, 'fast/real.html', summary(RELIABLE))
        self._cache(build_id, 'fast/none.html', None)
        classification = false_positive.classify(
            self.connection,
            fixtures.StubHistory({'fast/pre.html': AT_THRESHOLD, 'fast/real.html': RELIABLE},
                                 unavailable={'fast/unknown.html'}),
            self.stored_build(build_id),
        )
        verdicts = [entry.verdict for entry in self._explain(build_id)]
        self.assertEqual(verdicts.count(false_positive.PRE_EXISTING),
                         classification.surfaced_pre_existing)
        self.assertEqual(verdicts.count(false_positive.REAL), classification.surfaced_real)
        self.assertEqual(
            verdicts.count(false_positive.NO_HISTORY) + verdicts.count(false_positive.UNQUERIED),
            classification.surfaced_undetermined,
        )
        self.assertEqual(len(verdicts), classification.surfaced_total)
