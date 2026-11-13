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

"""The escape check: what main did with a convicted test after the change landed."""

from __future__ import annotations

import sqlite3
import time
import unittest
from typing import Optional
from unittest import mock

from ews_dashboard import config, results
from ews_dashboard.analysis import escapes
from tests import fixtures

LANDED_AT = fixtures.DEFAULT_BUILD_TIME + 86400
DAY = 86400
PULL_REQUEST = 12345
TEST = 'fast/a.html'


def _candidate(landed_at: int = LANDED_AT, tested_sha: str = 'a' * 40,
               newest_sha: str = 'a' * 40, build_id: int = 1) -> escapes.Candidate:
    return escapes.Candidate(
        build_id=build_id, test_name=TEST, rule=config.CLEAN_TREE,
        configuration=results.Configuration(suite='layout-tests', platform='mac', style='release'),
        pr_id=PULL_REQUEST, landed_at=landed_at, tested_sha=tested_sha, newest_sha=newest_sha,
    )


def _runs(failed: int, total: int, first_at: int) -> list:
    """`failed` unexpected failures and passes for the rest, ten minutes apart from `first_at`."""
    return [fixtures.run('TEXT' if index < failed else 'PASS', commit_at=first_at + index * 600)
            for index in range(total)]


class TestDecide(fixtures.DatabaseTest):
    """The judgement itself, over the runs either side of a landing."""

    def test_a_test_that_keeps_failing_after_the_landing_escaped(self) -> None:
        verdict = escapes.decide(
            [fixtures.run(commit_at=LANDED_AT - DAY)],
            [fixtures.run('TEXT', commit_at=LANDED_AT), fixtures.run('TEXT', commit_at=LANDED_AT + 60)],
        )
        self.assertEqual(verdict.verdict, escapes.ESCAPED)
        self.assertEqual((verdict.runs_after, verdict.failed_after), (2, 2))

    def test_a_lone_failure_over_a_clean_baseline_escaped_on_few_failures(self) -> None:
        """The population this check exists to find: main never failed it in 152 runs before the
        landing and failed it once in 96 after, which is an escape on thin evidence and not main's
        own failure."""
        verdict = escapes.decide(_runs(0, 152, LANDED_AT - escapes.ESCAPE_WINDOW_SECONDS),
                                 _runs(1, 96, LANDED_AT))
        self.assertEqual(verdict.verdict, escapes.ESCAPED)
        self.assertEqual(escapes.rarity_for_counts(verdict.runs_after, verdict.failed_after),
                         escapes.RARE)
        self.assertEqual((verdict.runs_before, verdict.failed_before), (152, 0))
        self.assertEqual((verdict.runs_after, verdict.failed_after), (96, 1))

    def test_a_baseline_that_failed_once_is_main_s_however_bad_the_window_after_is(self) -> None:
        """The baseline decides whose failure it is: main was failing this before the change existed,
        so no rate after the landing can lay it at the change's door."""
        verdict = escapes.decide(_runs(1, 96, LANDED_AT - escapes.ESCAPE_WINDOW_SECONDS),
                                 _runs(96, 96, LANDED_AT))
        self.assertEqual(verdict.verdict, escapes.FAILS_ON_MAIN)
        self.assertEqual((verdict.failed_before, verdict.failed_after), (1, 96))

    def test_a_clean_baseline_over_the_threshold_is_still_a_plain_escape(self) -> None:
        verdict = escapes.decide(_runs(0, 96, LANDED_AT - escapes.ESCAPE_WINDOW_SECONDS),
                                 _runs(60, 96, LANDED_AT))
        self.assertEqual(verdict.verdict, escapes.ESCAPED)

    def test_an_escape_on_few_failures_is_an_answer_and_not_an_absence_of_one(self) -> None:
        """The evidence is thinner, not missing, so it belongs in the rate rather than beside it."""
        self.assertNotIn(escapes.ESCAPED, escapes.UNDECIDED_VERDICTS)
        self.assertIn(escapes.ESCAPED, escapes.VERDICTS)

    def test_how_rarely_a_test_failed_is_no_verdict_of_its_own(self) -> None:
        """One stored answer, with the rate read off the counts beside it: a second verdict name is
        what let a stored rarity disagree with the runs it was taken from."""
        self.assertEqual(len([verdict for verdict in escapes.VERDICTS if 'ESCAPE' in verdict]), 1)
        self.assertEqual(sorted(escapes.VERDICT_DESCRIPTIONS), sorted(escapes.VERDICTS))

    def test_a_rate_exactly_at_the_threshold_is_a_strong_escape(self) -> None:
        """The boundary belongs to the strong side, as the sentence a reader gets says it does."""
        self.assertEqual(escapes.rarity_for_counts(4, 2), escapes.STRONG)
        self.assertEqual(escapes.rarity_for_counts(96, 47), escapes.RARE)

    def test_a_test_failing_less_often_than_the_threshold_over_a_clean_baseline_escaped(self) -> None:
        watched = [fixtures.run('TEXT', commit_at=LANDED_AT)]
        watched += [fixtures.run(commit_at=LANDED_AT + minute * 60) for minute in range(1, 5)]
        verdict = escapes.decide([fixtures.run(commit_at=LANDED_AT - DAY)], watched)
        self.assertEqual(verdict.verdict, escapes.ESCAPED)
        self.assertEqual(escapes.rarity_for_counts(verdict.runs_after, verdict.failed_after),
                         escapes.RARE)
        self.assertEqual((verdict.runs_after, verdict.failed_after), (5, 1))

    def test_a_clean_window_after_the_landing_is_contained(self) -> None:
        verdict = escapes.decide([fixtures.run(commit_at=LANDED_AT - DAY)],
                                 [fixtures.run(commit_at=LANDED_AT)])
        self.assertEqual(verdict.verdict, escapes.CONTAINED)

    def test_a_clean_window_needs_no_baseline_to_be_contained(self) -> None:
        """Nothing failed after the landing, so nothing escaped, whatever main did before it."""
        self.assertEqual(escapes.decide([], [fixtures.run(commit_at=LANDED_AT)]).verdict,
                         escapes.CONTAINED)

    def test_a_baseline_as_broken_as_a_regression_is_not_an_escape(self) -> None:
        """Main was failing it in the share a regression needs before the landing, so even a window
        that fails every run after cannot be laid at this change's door."""
        verdict = escapes.decide(_runs(4, 4, LANDED_AT - escapes.ESCAPE_WINDOW_SECONDS),
                                 _runs(4, 4, LANDED_AT))
        self.assertEqual(verdict.verdict, escapes.FAILS_ON_MAIN)
        self.assertEqual((verdict.runs_before, verdict.failed_before), (4, 4))

    def test_a_baseline_exactly_at_the_threshold_is_not_an_escape(self) -> None:
        verdict = escapes.decide(_runs(2, 4, LANDED_AT - escapes.ESCAPE_WINDOW_SECONDS),
                                 _runs(1, 1, LANDED_AT))
        self.assertEqual(verdict.verdict, escapes.FAILS_ON_MAIN)

    def test_a_regression_over_a_baseline_that_only_flaked_is_main_s_failure(self) -> None:
        """One failure in the baseline is still main failing the test without the change, so the
        conviction is corroborated however hard the test fails afterwards."""
        verdict = escapes.decide(_runs(1, 40, LANDED_AT - escapes.ESCAPE_WINDOW_SECONDS),
                                 _runs(40, 40, LANDED_AT))
        self.assertEqual(verdict.verdict, escapes.FAILS_ON_MAIN)
        self.assertEqual((verdict.failed_before, verdict.failed_after), (1, 40))

    def test_a_baseline_flaking_at_the_rate_it_keeps_after_corroborates_the_build(self) -> None:
        """A test flaking either side of the landing at a similar low rate is exactly the flakiness
        the build was told it was, so it is decided rather than counted nowhere."""
        verdict = escapes.decide(_runs(6, 88, LANDED_AT - escapes.ESCAPE_WINDOW_SECONDS),
                                 _runs(14, 99, LANDED_AT))
        self.assertEqual(verdict.verdict, escapes.FAILS_ON_MAIN)

    def test_an_empty_window_after_the_landing_answers_nothing_whatever_the_baseline(self) -> None:
        """NO_RUNS is decided before the baseline is read, so a broken main cannot mask it."""
        verdict = escapes.decide(_runs(4, 4, LANDED_AT - escapes.ESCAPE_WINDOW_SECONDS), [])
        self.assertEqual(verdict.verdict, escapes.NO_RUNS)

    def test_a_clean_window_is_contained_whatever_the_baseline(self) -> None:
        verdict = escapes.decide(_runs(4, 4, LANDED_AT - escapes.ESCAPE_WINDOW_SECONDS),
                                 [fixtures.run(commit_at=LANDED_AT)])
        self.assertEqual(verdict.verdict, escapes.CONTAINED)

    def test_a_failure_with_nothing_before_it_is_disclosed_rather_than_called_an_escape(self) -> None:
        verdict = escapes.decide([], [fixtures.run('TEXT', commit_at=LANDED_AT)])
        self.assertEqual(verdict.verdict, escapes.NO_BASELINE)

    def test_an_empty_window_after_the_landing_answers_nothing(self) -> None:
        verdict = escapes.decide([fixtures.run(commit_at=LANDED_AT - DAY)], [])
        self.assertEqual(verdict.verdict, escapes.NO_RUNS)

    def test_a_failure_main_expects_is_not_a_failure(self) -> None:
        """An expected failure is main failing to order, so counting it would convict every rule of
        an escape it had nothing to do with."""
        verdict = escapes.decide(
            [fixtures.run(commit_at=LANDED_AT - DAY)],
            [fixtures.run('TEXT', expected='PASS TEXT', commit_at=LANDED_AT)],
        )
        self.assertEqual(verdict.verdict, escapes.CONTAINED)


class TestStrength(unittest.TestCase):
    """The Wilson-bounded rate `strength_for_counts` ranks escapes by, ahead of the raw share."""

    def test_no_runs_after_the_landing_answers_nothing(self) -> None:
        self.assertIsNone(escapes.strength_for_counts(0, 0))

    def test_nine_of_fourteen_is_the_bounded_rate_not_the_raw_one(self) -> None:
        self.assertAlmostEqual(escapes.strength_for_counts(14, 9), 0.426, places=3)

    def test_no_failures_bounds_to_zero(self) -> None:
        self.assertEqual(escapes.strength_for_counts(8, 0), 0.0)

    def test_the_bound_orders_thin_and_thick_evidence_the_raw_rate_gets_backwards(self) -> None:
        """1 failure in 8 runs is 12.5% raw against 7 in 98 at 7.1% raw, which ranks the thin evidence
        above the thick evidence; the bound sinks the thin one below the thick one instead."""
        thin = escapes.strength_for_counts(8, 1)
        thick = escapes.strength_for_counts(98, 7)
        self.assertLess(thin, thick)


class TestRedecided(fixtures.DatabaseTest):
    """Deciding a stored row again from the counts it kept, which is all the migration has."""

    def test_a_stale_verdict_is_named_by_the_counts_rather_than_by_what_was_stored(self) -> None:
        self.assertEqual(escapes.redecided(escapes.FAILS_ON_MAIN, 152, 0, 96, 1), escapes.ESCAPED)

    def test_a_diverged_verdict_is_left_alone_because_it_stored_no_counts(self) -> None:
        """It is reached before any run is asked for, so its zeroes would read as NO_RUNS."""
        self.assertEqual(escapes.redecided(escapes.TREE_DIVERGED, 0, 0, 0, 0),
                         escapes.TREE_DIVERGED)

    def test_a_verdict_the_counts_still_support_is_unchanged(self) -> None:
        self.assertEqual(escapes.redecided(escapes.FAILS_ON_MAIN, 88, 6, 99, 14),
                         escapes.FAILS_ON_MAIN)


class TestTally(fixtures.DatabaseTest):
    """What a window's verdicts come to, and which of them the escape rate is taken over."""

    def test_a_test_main_fails_without_the_change_is_counted_in_the_rate(self) -> None:
        """Main failing a test whether the change is there or not vindicates the conviction, so it
        belongs in the denominator rather than among the convictions main answered nothing about."""
        self.assertNotIn(escapes.FAILS_ON_MAIN, escapes.UNDECIDED_VERDICTS)
        tally = escapes.Tally(
            by_verdict={escapes.ESCAPED: 0, escapes.FAILS_ON_MAIN: 5, escapes.CONTAINED: 39,
                        escapes.NO_RUNS: 2},
            unaskable={},
        )
        self.assertEqual((tally.decided, tally.undecided), (44, 2))
        self.assertEqual(tally.escape_rate_pct, 0.0)

    def test_what_was_asked_counts_the_convictions_no_answer_came_back_about(self) -> None:
        """The buckets divide up every conviction main was asked about, so their total has to hold
        the undecided ones the rate leaves out."""
        tally = escapes.Tally(
            by_verdict={escapes.ESCAPED: 0, escapes.FAILS_ON_MAIN: 5, escapes.CONTAINED: 39,
                        escapes.NO_RUNS: 2},
            unaskable={escapes.NOT_LANDED: 7},
        )
        self.assertEqual(tally.asked, 46)

    def test_a_verdict_name_this_code_retired_is_counted_rather_than_dropped(self) -> None:
        """Rebuilding the buckets from VERDICTS alone once took the rows stored under a since-retired
        name out of every total on the page silently, so what is asked has to hold them."""
        tally = escapes.Tally(
            by_verdict={escapes.ESCAPED: 1, escapes.FAILS_ON_MAIN: 5, escapes.CONTAINED: 39,
                        escapes.NO_RUNS: 2},
            unaskable={},
            unrecognised={'FLAKY_ON_MAIN': 10, 'ALREADY_FAILING': 5},
        )
        self.assertEqual(tally.unrecognised_total, 15)
        self.assertEqual(tally.asked, 62)
        self.assertEqual(tally.asked, tally.decided + tally.undecided + tally.unrecognised_total)

    def test_a_retired_name_is_kept_out_of_the_rate_it_cannot_be_graded_by(self) -> None:
        """It answers nothing this code can read, so it belongs in no numerator and no denominator;
        the migration is what folds it into the bucket that replaced it."""
        tally = escapes.Tally(
            by_verdict={escapes.ESCAPED: 1, escapes.CONTAINED: 3},
            unaskable={},
            unrecognised={'FLAKY_ON_MAIN': 96},
        )
        self.assertEqual((tally.decided, tally.undecided), (4, 0))
        self.assertEqual(tally.escape_rate_pct, 25.0)

    def test_the_headline_counts_every_way_a_conviction_can_escape(self) -> None:
        """A conviction that excused something main had not been failing escaped whether the
        failures after were many or few, so the one bucket holds both and the rate is over it."""
        tally = escapes.Tally(
            by_verdict={escapes.ESCAPED: 5, escapes.CONTAINED: 15, escapes.NO_RUNS: 4},
            unaskable={},
        )
        self.assertEqual((tally.escaped, tally.decided), (5, 20))
        self.assertEqual(tally.escape_rate_pct, 25.0)

    def test_the_buckets_still_divide_up_what_was_asked(self) -> None:
        tally = escapes.Tally(
            by_verdict={escapes.ESCAPED: 5, escapes.CONTAINED: 15, escapes.NO_RUNS: 4},
            unaskable={escapes.NOT_LANDED: 7},
            unrecognised={'FLAKY_ON_MAIN': 6},
        )
        self.assertEqual(tally.asked, tally.decided + tally.undecided + tally.unrecognised_total)
        self.assertEqual(tally.asked, 30)

    def test_a_tally_with_nothing_unrecognised_counts_exactly_what_it_used_to(self) -> None:
        """The ordinary case, and the one every other page number is read under."""
        tally = escapes.Tally(by_verdict={escapes.ESCAPED: 1, escapes.NO_RUNS: 2}, unaskable={})
        self.assertEqual((tally.unrecognised_total, tally.asked), (0, 3))


class TestByVerdict(fixtures.DatabaseTest):
    """What the stored rows come to, including any stored under a name since retired."""

    def _store(self, number: int, verdict: str) -> None:
        build_id = self.store_build(number, flaky={TEST: config.CLEAN_TREE}, pr_id=number,
                                    pr_title='A change that landed', sha='a' * 40)
        with self.connection:
            self.connection.execute(
                '''INSERT INTO escape_verdicts (
                    build_id, test_name, verdict, runs_before, failed_before, runs_after,
                    failed_after, window_ends_at, decided_at
                ) VALUES (?,?,?,?,?,?,?,?,?)''',
                (build_id, TEST, verdict, 4, 0, 6, 2,
                 LANDED_AT + escapes.ESCAPE_WINDOW_SECONDS, LANDED_AT),
            )

    def _tally(self) -> escapes.Tally:
        return escapes.tally(self.connection, fixtures.DEFAULT_BUILD_TIME - DAY,
                             fixtures.DEFAULT_BUILD_TIME + DAY)

    def test_every_bucket_is_reported_even_when_nothing_reached_it(self) -> None:
        self._store(1, escapes.CONTAINED)
        by_verdict = escapes.by_verdict(self.connection, fixtures.DEFAULT_BUILD_TIME - DAY,
                                        fixtures.DEFAULT_BUILD_TIME + DAY)
        self.assertEqual(sorted(by_verdict), sorted(escapes.VERDICTS))
        self.assertEqual(by_verdict[escapes.ESCAPED], 0)

    def test_a_verdict_no_bucket_matches_is_reported_separately(self) -> None:
        """A row can only hold one on a table whose CHECK constraint predates the rename, so the
        database-level case lives with the migration that fixes it; this pins the empty answer."""
        self._store(1, escapes.CONTAINED)
        self.assertEqual(self._tally().unrecognised, {})


class TestSubcategories(fixtures.DatabaseTest):
    """How the escapes split under the one headline number, twice over."""

    def _escape(self, number: int, runs_after: int, failed_after: int,
                recent_runs: Optional[int] = None, recent_failed: Optional[int] = None,
                recent_checked_at: Optional[int] = None,
                verdict: str = escapes.ESCAPED) -> None:
        test_name = f'fast/{number}.html'
        build_id = self.store_build(number, flaky={test_name: config.CLEAN_TREE}, pr_id=number,
                                    pr_title='A change that landed', sha='a' * 40)
        with self.connection:
            self.connection.execute(
                '''INSERT INTO escape_verdicts (
                    build_id, test_name, verdict, runs_before, failed_before, runs_after,
                    failed_after, recent_runs, recent_failed, recent_checked_at,
                    window_ends_at, decided_at
                ) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)''',
                (build_id, test_name, verdict, 152, 0, runs_after, failed_after, recent_runs,
                 recent_failed, recent_checked_at, LANDED_AT + escapes.ESCAPE_WINDOW_SECONDS,
                 LANDED_AT),
            )

    def _subcategories(self, **scope: object) -> escapes.Subcategories:
        return escapes.escape_subcategories(self.connection, fixtures.DEFAULT_BUILD_TIME - DAY,
                                            fixtures.DEFAULT_BUILD_TIME + DAY, **scope)

    def test_each_split_adds_up_to_the_escapes_it_divides(self) -> None:
        """One of each currency state, so the four are shown to partition the bucket rather than to
        happen to agree with it on a fixture missing one."""
        checked_at = fixtures.DEFAULT_BUILD_TIME + 2 * DAY
        self._escape(1, runs_after=96, failed_after=90, recent_runs=18, recent_failed=11,
                     recent_checked_at=checked_at)
        self._escape(2, runs_after=96, failed_after=1, recent_runs=22, recent_failed=0,
                     recent_checked_at=checked_at)
        self._escape(3, runs_after=40, failed_after=2, recent_runs=0, recent_failed=0,
                     recent_checked_at=checked_at)
        self._escape(4, runs_after=40, failed_after=2)

        split = self._subcategories()

        self.assertEqual(
            (split.still_failing, split.recovered, split.not_run_lately, split.unchecked),
            (1, 1, 1, 1),
        )
        self.assertEqual((split.strong, split.rare), (1, 3))
        self.assertEqual(split.total, 4)
        self.assertEqual(split.rate_total, split.total)

    def test_an_escape_nothing_asked_about_is_neither_still_failing_nor_recovered(self) -> None:
        self._escape(1, runs_after=96, failed_after=1)
        split = self._subcategories()
        self.assertEqual(
            (split.still_failing, split.recovered, split.not_run_lately, split.unchecked),
            (0, 0, 0, 1),
        )

    def test_an_escape_main_has_not_run_lately_is_not_counted_as_a_recovery(self) -> None:
        """Main demonstrating nothing is not main demonstrating a fix, and the recovered count is the
        one a reader takes as reassurance."""
        self._escape(1, runs_after=96, failed_after=1, recent_runs=0, recent_failed=0,
                     recent_checked_at=fixtures.DEFAULT_BUILD_TIME + 2 * DAY)
        split = self._subcategories()
        self.assertEqual(
            (split.still_failing, split.recovered, split.not_run_lately, split.unchecked),
            (0, 0, 1, 0),
        )

    def test_no_escape_can_fall_outside_the_rate_split(self) -> None:
        """An ESCAPED row with no run after the landing would be counted by the currency split and by
        neither rate bucket; `verdict_for_counts` cannot produce one, which is what makes the two
        rate buckets a partition rather than a pair of filters."""
        self.assertEqual(escapes.verdict_for_counts(152, 0, 0, 0), escapes.NO_RUNS)
        self.assertIsNotNone(escapes.rarity_for_counts(1, 1))

    def test_no_other_verdict_is_counted_in_the_split(self) -> None:
        self._escape(1, runs_after=96, failed_after=0, verdict=escapes.CONTAINED)
        self._escape(2, runs_after=96, failed_after=48, verdict=escapes.FAILS_ON_MAIN)
        self.assertEqual(self._subcategories().total, 0)

    def test_a_queue_the_page_is_narrowed_to_narrows_the_split_too(self) -> None:
        self._escape(1, runs_after=96, failed_after=90)
        self.assertEqual(self._subcategories(builders=(fixtures.GTK_BUILDER,)).total, 0)
        self.assertEqual(self._subcategories(builders=(fixtures.LAYOUT_BUILDER,)).total, 1)


class TestAssessOne(fixtures.DatabaseTest):
    def test_the_landing_commit_belongs_to_the_window_after_it_and_not_to_the_baseline(self) -> None:
        """The endpoint's bounds are not trusted to exclude their endpoints, so a run of the landing
        commit itself must not answer as the baseline it is compared against."""
        history = fixtures.StubRunHistory({TEST: [fixtures.run('TEXT', commit_at=LANDED_AT)]})
        verdict = escapes.assess_one(history, _candidate())
        self.assertEqual(verdict.verdict, escapes.NO_BASELINE)
        self.assertEqual((verdict.runs_before, verdict.runs_after), (0, 1))

    def test_both_windows_span_the_configured_number_of_days(self) -> None:
        history = fixtures.StubRunHistory({TEST: []})
        escapes.assess_one(history, _candidate())
        self.assertEqual(
            [(query.after, query.before) for query in history.queries],
            [(LANDED_AT - escapes.ESCAPE_WINDOW_SECONDS, LANDED_AT),
             (LANDED_AT, LANDED_AT + escapes.ESCAPE_WINDOW_SECONDS)],
        )

    def test_a_pull_request_that_moved_after_the_conviction_is_not_asked_about(self) -> None:
        """A later build tested a different head, so main holds code this conviction was never made
        on and neither answer would be about it."""
        history = fixtures.StubRunHistory({TEST: [fixtures.run('TEXT', commit_at=LANDED_AT)]})
        verdict = escapes.assess_one(history, _candidate(tested_sha='a' * 40, newest_sha='b' * 40))
        self.assertEqual(verdict.verdict, escapes.TREE_DIVERGED)
        self.assertEqual(history.queries, [])


class TestAssess(fixtures.DatabaseTest):
    """The stored pass over a window of convictions."""

    def _convict(self, number: int = 1, started_at: int = fixtures.DEFAULT_BUILD_TIME,
                 sha: str = 'a' * 40) -> int:
        return self.store_build(number, flaky={TEST: config.CLEAN_TREE}, pr_id=PULL_REQUEST,
                                pr_title='A change that landed', sha=sha, started_at=started_at)

    def _assess(self, history: fixtures.StubRunHistory) -> dict:
        return dict(escapes.assess(self.connection, history, fixtures.DEFAULT_BUILD_TIME - DAY,
                                   fixtures.DEFAULT_BUILD_TIME + DAY))

    def test_a_conviction_whose_pull_request_landed_is_decided_and_stored(self) -> None:
        self._convict()
        self.store_landing(PULL_REQUEST, landed_at=LANDED_AT)
        history = fixtures.StubRunHistory({TEST: [
            fixtures.run(commit_at=LANDED_AT - DAY),
            fixtures.run('TEXT', commit_at=LANDED_AT),
        ]})
        self.assertEqual(self._assess(history), {escapes.ESCAPED: 1})
        stored = self.connection.execute('SELECT * FROM escape_verdicts').fetchall()
        self.assertEqual([(row['test_name'], row['verdict']) for row in stored],
                         [(TEST, escapes.ESCAPED)])

    def test_a_settled_verdict_is_not_asked_about_again(self) -> None:
        self._convict()
        self.store_landing(PULL_REQUEST, landed_at=LANDED_AT)
        history = fixtures.StubRunHistory({TEST: [fixtures.run(commit_at=LANDED_AT)]})
        self._assess(history)
        asked = len(history.queries)
        self.assertEqual(self._assess(history), {escapes.CONTAINED: 1})
        self.assertEqual(len(history.queries), asked)

    def test_a_verdict_reached_before_its_window_closed_is_asked_again(self) -> None:
        """The runs that turn CONTAINED into ESCAPED arrive after the window's last commit, so a
        verdict decided while it was still filling cannot be kept."""
        self._convict()
        self.store_landing(PULL_REQUEST, landed_at=int(time.time()) - 60)
        history = fixtures.StubRunHistory({TEST: []})
        self._assess(history)
        asked = len(history.queries)
        self._assess(history)
        self.assertGreater(len(history.queries), asked)

    def test_a_conviction_on_a_pull_request_with_no_landing_reaches_no_network(self) -> None:
        self._convict()
        history = fixtures.StubRunHistory({TEST: [fixtures.run('TEXT', commit_at=LANDED_AT)]})
        self.assertEqual(self._assess(history), {})
        self.assertEqual(history.queries, [])

    def test_an_ambiguous_title_is_not_asked_about(self) -> None:
        self._convict()
        self.store_landing(PULL_REQUEST, status='ambiguous', matches=14)
        history = fixtures.StubRunHistory({TEST: [fixtures.run('TEXT', commit_at=LANDED_AT)]})
        self.assertEqual(self._assess(history), {})
        self.assertEqual(history.queries, [])

    def test_an_unreachable_results_service_is_counted_and_not_stored(self) -> None:
        self._convict()
        self.store_landing(PULL_REQUEST, landed_at=LANDED_AT)
        history = fixtures.StubRunHistory({}, unavailable={TEST})
        self.assertEqual(self._assess(history), {escapes.UNAVAILABLE: 1})
        self.assertEqual(self.connection.execute(
            'SELECT COUNT(*) FROM escape_verdicts').fetchone()[0], 0)

    def test_a_conviction_made_on_a_head_a_later_build_replaced_is_not_asked_about(self) -> None:
        self._convict(number=1, sha='a' * 40, started_at=fixtures.DEFAULT_BUILD_TIME)
        self._convict(number=2, sha='b' * 40, started_at=fixtures.DEFAULT_BUILD_TIME + 600)
        self.store_landing(PULL_REQUEST, landed_at=LANDED_AT)
        history = fixtures.StubRunHistory({TEST: [
            fixtures.run(commit_at=LANDED_AT - DAY),
            fixtures.run('TEXT', commit_at=LANDED_AT),
        ]})
        self.assertEqual(self._assess(history),
                         {escapes.TREE_DIVERGED: 1, escapes.ESCAPED: 1})


class TestSupersedingHead(fixtures.DatabaseTest):
    """Which build of a pull request is allowed to say the conviction was made on superseded code.

    EWS keeps building a pull request after it has landed, so the newest build of one is routinely a
    build of code main already had. Such a build cannot have superseded the tree that landed, and
    counting it as divergence retires a conviction main could have graded.
    """

    def _convict(self, number: int, sha: str, started_at: int) -> int:
        return self.store_build(number, flaky={TEST: config.CLEAN_TREE}, pr_id=PULL_REQUEST,
                                pr_title='A change that landed', sha=sha, started_at=started_at)

    def _candidate(self, build_id: int) -> escapes.Candidate:
        listed = [one for one in escapes.candidates(self.connection,
                                                    fixtures.DEFAULT_BUILD_TIME - DAY,
                                                    LANDED_AT + DAY)
                  if one.build_id == build_id]
        self.assertEqual(len(listed), 1)
        return listed[0]

    def test_a_build_started_after_the_landing_is_not_the_head_that_landed(self) -> None:
        convicted = self._convict(1, 'a' * 40, fixtures.DEFAULT_BUILD_TIME)
        self._convict(2, 'b' * 40, LANDED_AT + 600)
        self.store_landing(PULL_REQUEST, landed_at=LANDED_AT)
        candidate = self._candidate(convicted)
        self.assertEqual(candidate.newest_sha, 'a' * 40)
        self.assertFalse(candidate.diverged)

    def test_a_build_started_before_the_landing_is(self) -> None:
        convicted = self._convict(1, 'a' * 40, fixtures.DEFAULT_BUILD_TIME)
        self._convict(2, 'b' * 40, fixtures.DEFAULT_BUILD_TIME + 600)
        self.store_landing(PULL_REQUEST, landed_at=LANDED_AT)
        candidate = self._candidate(convicted)
        self.assertEqual(candidate.newest_sha, 'b' * 40)
        self.assertTrue(candidate.diverged)

    def test_a_conviction_a_post_landing_build_followed_is_graded_rather_than_retired(self) -> None:
        self._convict(1, 'a' * 40, fixtures.DEFAULT_BUILD_TIME)
        self._convict(2, 'b' * 40, LANDED_AT + 600)
        self.store_landing(PULL_REQUEST, landed_at=LANDED_AT)
        history = fixtures.StubRunHistory({TEST: [
            fixtures.run(commit_at=LANDED_AT - DAY),
            fixtures.run('TEXT', commit_at=LANDED_AT),
        ]})
        self.assertEqual(dict(escapes.assess(self.connection, history,
                                             fixtures.DEFAULT_BUILD_TIME - DAY,
                                             fixtures.DEFAULT_BUILD_TIME + DAY)),
                         {escapes.ESCAPED: 1})


class TestCurrency(fixtures.DatabaseTest):
    """Whether main is still failing an escaped test, asked over a fresh window ending now."""

    def setUp(self) -> None:
        super().setUp()
        self.build_id = self.store_build(1, flaky={TEST: config.CLEAN_TREE}, pr_id=PULL_REQUEST,
                                         pr_title='A change that landed', sha='a' * 40)
        self.store_landing(PULL_REQUEST, landed_at=LANDED_AT)
        self.now = int(time.time())

    def _store_escape(self, verdict: str = escapes.ESCAPED, recent_runs: Optional[int] = None,
                      recent_failed: Optional[int] = None,
                      recent_checked_at: Optional[int] = None) -> None:
        with self.connection:
            self.connection.execute(
                '''INSERT OR REPLACE INTO escape_verdicts (
                    build_id, test_name, verdict, runs_before, failed_before, runs_after,
                    failed_after, recent_runs, recent_failed, recent_checked_at,
                    window_ends_at, decided_at
                ) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)''',
                (self.build_id, TEST, verdict, 152, 0, 96, 1, recent_runs, recent_failed,
                 recent_checked_at, LANDED_AT + escapes.ESCAPE_WINDOW_SECONDS, LANDED_AT),
            )

    def _stored(self) -> sqlite3.Row:
        return self.connection.execute(
            'SELECT recent_runs, recent_failed, recent_checked_at FROM escape_verdicts '
            'WHERE build_id = ? AND test_name = ?', (self.build_id, TEST),
        ).fetchone()

    def _candidate(self) -> escapes.Candidate:
        return _candidate(build_id=self.build_id)

    def test_an_escape_checked_within_the_day_is_not_asked_again(self) -> None:
        """Roughly sixty escapes are re-read on every pass, so the answer has to stand for a while or
        the check becomes a second full sweep of results.webkit.org."""
        self._store_escape(recent_runs=40, recent_failed=3,
                           recent_checked_at=self.now - config.CURRENCY_TTL_SECONDS + 600)
        history = fixtures.StubRunHistory({TEST: [fixtures.run('TEXT', commit_at=self.now - 3600)]})

        self.assertFalse(escapes.check_currency(self.connection, history, self._candidate(),
                                                self.now))
        self.assertEqual(history.queries, [])
        self.assertEqual(self._stored()['recent_failed'], 3)

    def test_an_escape_checked_longer_ago_than_the_ttl_is_asked_again(self) -> None:
        self._store_escape(recent_runs=40, recent_failed=3,
                           recent_checked_at=self.now - config.CURRENCY_TTL_SECONDS - 600)
        history = fixtures.StubRunHistory({TEST: [
            fixtures.run('TEXT', commit_at=self.now - 3600),
            fixtures.run(commit_at=self.now - 7200),
        ]})

        self.assertTrue(escapes.check_currency(self.connection, history, self._candidate(),
                                               self.now))
        self.assertEqual([(query.after, query.before) for query in history.queries],
                         [(self.now - escapes.CURRENCY_WINDOW_SECONDS, self.now)])
        row = self._stored()
        self.assertEqual((row['recent_runs'], row['recent_failed'], row['recent_checked_at']),
                         (2, 1, self.now))

    def test_the_window_asked_about_ends_now_rather_than_at_the_landing(self) -> None:
        """A window fixed to the landing cannot say whether the regression is still there, however
        wide it is."""
        self._store_escape()
        history = fixtures.StubRunHistory({TEST: []})

        escapes.check_currency(self.connection, history, self._candidate(), self.now)

        asked = history.queries[0]
        self.assertEqual(asked.before, self.now)
        self.assertEqual(self.now - asked.after, config.CURRENCY_DAYS * 86400)
        self.assertGreater(asked.after, LANDED_AT)

    def test_an_escape_nothing_has_asked_about_is_asked(self) -> None:
        self._store_escape()
        history = fixtures.StubRunHistory({TEST: [fixtures.run(commit_at=self.now - 600)]})

        self.assertTrue(escapes.check_currency(self.connection, history, self._candidate(),
                                               self.now))
        self.assertEqual(self._stored()['recent_checked_at'], self.now)

    def test_an_outage_leaves_the_escape_unchecked_rather_than_recovered(self) -> None:
        self._store_escape()
        history = fixtures.StubRunHistory({}, unavailable={TEST})

        self.assertFalse(escapes.check_currency(self.connection, history, self._candidate(),
                                                self.now))
        row = self._stored()
        self.assertIsNone(row['recent_checked_at'])
        self.assertEqual(escapes.currency_for_counts(row['recent_runs'], row['recent_failed'],
                                                     row['recent_checked_at']), escapes.UNCHECKED)

    def test_no_failure_in_the_recent_window_reads_as_main_having_stopped(self) -> None:
        self._store_escape()
        history = fixtures.StubRunHistory({TEST: [fixtures.run(commit_at=self.now - 600),
                                                  fixtures.run(commit_at=self.now - 1200)]})

        escapes.check_currency(self.connection, history, self._candidate(), self.now)

        row = self._stored()
        self.assertEqual((row['recent_runs'], row['recent_failed']), (2, 0))
        self.assertEqual(escapes.currency_for_counts(row['recent_runs'], row['recent_failed'],
                                                     row['recent_checked_at']), escapes.RECOVERED)

    def test_a_window_main_ran_nothing_in_reads_as_unmeasured_and_not_as_a_recovery(self) -> None:
        """Zero runs and zero failures out of them are the same two numbers a recovery stores, so
        without the run count this state would have been reported as main having fixed the test."""
        self._store_escape()
        history = fixtures.StubRunHistory({TEST: []})

        escapes.check_currency(self.connection, history, self._candidate(), self.now)

        row = self._stored()
        self.assertEqual((row['recent_runs'], row['recent_failed'], row['recent_checked_at']),
                         (0, 0, self.now))
        self.assertEqual(escapes.currency_for_counts(row['recent_runs'], row['recent_failed'],
                                                     row['recent_checked_at']),
                         escapes.NOT_RUN_LATELY)

    def test_no_two_currency_states_are_ever_the_same_answer(self) -> None:
        """No boolean is stored for exactly this reason: zero failures out of some runs, zero runs to
        fail, and no measurement at all are three different answers, and two of them are not answers.
        """
        states = (escapes.currency_for_counts(40, 3, self.now),
                  escapes.currency_for_counts(44, 0, self.now),
                  escapes.currency_for_counts(0, 0, self.now),
                  escapes.currency_for_counts(None, None, None))
        self.assertEqual(states, (escapes.STILL_FAILING, escapes.RECOVERED,
                                  escapes.NOT_RUN_LATELY, escapes.UNCHECKED))
        self.assertEqual(len(set(states)), 4)

    def test_only_the_escapes_are_asked_whether_main_is_still_failing_them(self) -> None:
        """Tens of escapes against thousands of convictions: a currency query per conviction is the
        hour-long pass this dashboard already learned to avoid."""
        history = fixtures.StubRunHistory({TEST: [
            fixtures.run(commit_at=LANDED_AT - DAY),
            fixtures.run(commit_at=LANDED_AT),
            fixtures.run('TEXT', commit_at=self.now - 3600),
        ]})

        outcomes = dict(escapes.assess(self.connection, history, fixtures.DEFAULT_BUILD_TIME - DAY,
                                       fixtures.DEFAULT_BUILD_TIME + DAY))

        self.assertEqual(outcomes, {escapes.CONTAINED: 1})
        self.assertEqual([(query.after, query.before) for query in history.queries],
                         [(LANDED_AT - escapes.ESCAPE_WINDOW_SECONDS, LANDED_AT),
                          (LANDED_AT, LANDED_AT + escapes.ESCAPE_WINDOW_SECONDS)])
        self.assertIsNone(self._stored()['recent_checked_at'])

    def test_an_escape_the_assess_pass_decides_is_checked_in_the_same_pass(self) -> None:
        history = fixtures.StubRunHistory({TEST: [
            fixtures.run(commit_at=LANDED_AT - DAY),
            fixtures.run('TEXT', commit_at=LANDED_AT),
            fixtures.run('TEXT', commit_at=self.now - 3600),
        ]})

        outcomes = dict(escapes.assess(self.connection, history, fixtures.DEFAULT_BUILD_TIME - DAY,
                                       fixtures.DEFAULT_BUILD_TIME + DAY))

        self.assertEqual(outcomes, {escapes.ESCAPED: 1})
        row = self._stored()
        self.assertEqual((row['recent_runs'], row['recent_failed']), (1, 1))
        self.assertIsNotNone(row['recent_checked_at'])


class TestDamage(unittest.TestCase):
    """The raw rate `damage_for_counts` reads off the last currency check."""

    def test_none_runs_answers_nothing(self) -> None:
        self.assertIsNone(escapes.damage_for_counts(None, None))

    def test_zero_runs_answers_nothing(self) -> None:
        self.assertIsNone(escapes.damage_for_counts(0, 0))

    def test_none_failures_answers_nothing_rather_than_zero(self) -> None:
        """A row nobody has run the currency query against stores no failure count either, and 0%
        would claim an answer this row does not carry."""
        self.assertIsNone(escapes.damage_for_counts(40, None))

    def test_one_of_three_hundred_and_eleven_is_the_raw_rate(self) -> None:
        self.assertAlmostEqual(escapes.damage_for_counts(311, 1), 0.0032, places=4)


class TestUnaskable(fixtures.DatabaseTest):
    def test_convictions_main_cannot_be_asked_about_are_counted_by_why_not(self) -> None:
        self.store_build(1, flaky={TEST: config.CLEAN_TREE}, pr_id=1, pr_title='One')
        self.store_build(2, flaky={TEST: config.CLEAN_TREE}, pr_id=2, pr_title='Two')
        self.store_build(3, flaky={TEST: config.CLEAN_TREE}, pr_id=3, pr_title='Three')
        self.store_landing(2, status='not_landed', matches=0)
        self.store_landing(3, status='ambiguous', matches=9)
        self.assertEqual(
            escapes.unaskable(self.connection, fixtures.DEFAULT_BUILD_TIME - DAY,
                              fixtures.DEFAULT_BUILD_TIME + DAY),
            {escapes.NOT_LANDED: 1, escapes.AMBIGUOUS: 1, escapes.UNRESOLVED: 1},
        )

    def test_a_conviction_that_was_asked_about_is_not_counted_as_unaskable(self) -> None:
        self.store_build(1, flaky={TEST: config.CLEAN_TREE}, pr_id=1, pr_title='One')
        self.store_landing(1, landed_at=LANDED_AT)
        self.assertEqual(
            escapes.unaskable(self.connection, fixtures.DEFAULT_BUILD_TIME - DAY,
                              fixtures.DEFAULT_BUILD_TIME + DAY),
            {escapes.NOT_LANDED: 0, escapes.AMBIGUOUS: 0, escapes.UNRESOLVED: 0},
        )


def _conviction(verdict: str, **fields: object) -> escapes.Conviction:
    values = dict(
        test_name=TEST, rule=config.CLEAN_TREE, verdict=verdict, build_id=1,
        builder=fixtures.LAYOUT_BUILDER, builder_id=7, build_number=1, pr_id=PULL_REQUEST,
        configuration=results.Configuration(suite='layout-tests', platform='mac', style='release'),
        runs_before=4, failed_before=0, runs_after=6, failed_after=2,
        landed_at=LANDED_AT, window_ends_at=LANDED_AT + escapes.ESCAPE_WINDOW_SECONDS,
        tested_sha='a' * 40, newest_sha='b' * 40, heads=2, builds=3,
    )
    values.update(fields)
    return escapes.Conviction(**values)


def _prose(parts: 'tuple[escapes.Part, ...]') -> str:
    return ''.join(part.text for part in parts)


def _emphasised(parts: 'tuple[escapes.Part, ...]') -> 'list[str]':
    return [part.text for part in parts if part.emphasis]


class TestSentence(fixtures.DatabaseTest):
    """One sentence per verdict, so a count drilled into explains itself without a legend."""

    def test_a_fails_on_main_verdict_reports_both_rates(self) -> None:
        """The rate either side is where a reader now tells a flaky test from a broken one, so both
        have to be in the sentence."""
        self.assertIn('failed it 14 of 99 runs after the landing vs. 6 of 88 before',
                      _prose(escapes.sentence(_conviction(escapes.FAILS_ON_MAIN, runs_before=88,
                                                          failed_before=6, runs_after=99,
                                                          failed_after=14))))

    def test_a_fails_on_main_verdict_reads_the_same_for_a_baseline_main_was_broken_on(self) -> None:
        self.assertIn('failed it 90 of 96 runs after the landing vs. 90 of 96 before',
                      _prose(escapes.sentence(_conviction(escapes.FAILS_ON_MAIN, runs_before=96,
                                                          failed_before=90, runs_after=96,
                                                          failed_after=90))))

    def test_a_contained_verdict_says_main_never_failed_it(self) -> None:
        self.assertIn('ran it 6 times after the landing and never failed it',
                      _prose(escapes.sentence(_conviction(escapes.CONTAINED, failed_after=0))))

    def test_a_no_runs_verdict_says_nothing_ran_it(self) -> None:
        self.assertIn(f'No bot ran it on main in the {config.ESCAPE_WINDOW_DAYS} days',
                      _prose(escapes.sentence(_conviction(escapes.NO_RUNS, runs_after=0,
                                                          failed_after=0))))

    def test_a_no_baseline_verdict_says_nothing_ran_before_it(self) -> None:
        self.assertIn(f'nothing ran it in the {config.ESCAPE_WINDOW_DAYS} days before',
                      _prose(escapes.sentence(_conviction(escapes.NO_BASELINE, runs_before=0))))

    def test_a_diverged_verdict_names_both_heads_and_how_many_there_were(self) -> None:
        sentence = _prose(escapes.sentence(_conviction(escapes.TREE_DIVERGED)))
        self.assertIn(f'Convicted on head {"a" * 8}', sentence)
        self.assertIn(f'PR {PULL_REQUEST} was built 3 times across 2 heads', sentence)
        self.assertIn(f'landed as {"b" * 8}', sentence)

    def test_an_escaped_verdict_says_main_had_never_failed_it_before(self) -> None:
        self.assertIn(f'failed it 4 of 6 runs after the landing, at or above the '
                      f'{config.ESCAPE_FAILURE_PCT}% a strong escape needs, having never failed it '
                      'in the 4 runs before.',
                      _prose(escapes.sentence(_conviction(escapes.ESCAPED, failed_after=4))))

    def test_an_escape_on_few_failures_names_the_low_rate_as_the_reason_for_caution(self) -> None:
        sentence = _prose(escapes.sentence(_conviction(escapes.ESCAPED, runs_before=152,
                                                       failed_before=0, runs_after=96,
                                                       failed_after=1)))
        self.assertIn(f'failed it 1 of 96 runs after the landing, below the '
                      f'{config.ESCAPE_FAILURE_PCT}% a strong escape needs, so the escape rests on '
                      'few failures, having never failed it in the 152 runs before', sentence)
        self.assertIn(f'below the {config.ESCAPE_FAILURE_PCT}% a strong escape needs', sentence)
        self.assertIn('rests on few failures', sentence)

    def test_an_escape_main_is_still_failing_says_so_with_the_recent_counts(self) -> None:
        sentence = escapes.sentence(_conviction(escapes.ESCAPED, recent_runs=12, recent_failed=9,
                                                recent_checked_at=fixtures.DEFAULT_BUILD_TIME))
        self.assertIn(f'Main is still failing it, 9 of 12 runs in the last {config.CURRENCY_DAYS} '
                      'days.', _prose(sentence))
        self.assertIn('9 of 12', _emphasised(sentence))

    def test_an_escape_main_has_stopped_failing_says_that_instead(self) -> None:
        sentence = escapes.sentence(_conviction(escapes.ESCAPED, recent_runs=31, recent_failed=0,
                                                recent_checked_at=fixtures.DEFAULT_BUILD_TIME))
        self.assertIn('Main has stopped failing it: none of its 31 runs in the last '
                      f'{config.CURRENCY_DAYS} days did.', _prose(sentence))
        self.assertIn('31 runs', _emphasised(sentence))

    def test_an_escape_main_has_not_run_lately_says_that_and_nothing_about_failures(self) -> None:
        """The counts it would otherwise print are 0 of 0, which reads as a clean recent record while
        being no record at all."""
        sentence = escapes.sentence(_conviction(escapes.ESCAPED, recent_runs=0, recent_failed=0,
                                                recent_checked_at=fixtures.DEFAULT_BUILD_TIME))
        prose = _prose(sentence)
        self.assertIn(f'Main has not run it in the last {config.CURRENCY_DAYS} days, so whether the '
                      'failure is still there is unmeasured.', prose)
        self.assertNotIn('stopped failing it', prose)
        self.assertNotIn('0 runs', prose)

    def test_an_escape_nothing_has_asked_about_claims_nothing_either_way(self) -> None:
        """Saying main has stopped would be a recovery nobody measured, and saying it is still
        failing would be a regression nobody measured."""
        sentence = _prose(escapes.sentence(_conviction(escapes.ESCAPED)))
        self.assertNotIn('still failing it', sentence)
        self.assertNotIn('stopped failing it', sentence)
        self.assertNotIn('has not run it', sentence)
        self.assertNotIn(f'last {config.CURRENCY_DAYS} days', sentence)

    def test_a_diverged_verdict_with_no_head_recorded_omits_it(self) -> None:
        sentence = _prose(escapes.sentence(_conviction(escapes.TREE_DIVERGED, tested_sha=None,
                                                       newest_sha=None, pr_id=None)))
        self.assertNotIn('None', sentence)
        self.assertIn('the pull request was built 3 times', sentence)

    def test_a_sentence_carries_no_markup_of_its_own(self) -> None:
        """It interpolates test names, so the page has to keep autoescaping it: the emphasis is
        data and the template is what turns it into tags."""
        for verdict in escapes.VERDICTS:
            for part in escapes.sentence(_conviction(verdict)):
                self.assertIs(type(part.text), str)
                self.assertNotIn('<', part.text)

    def test_every_verdict_that_reports_counts_emphasises_them(self) -> None:
        """A page where one verdict bolds its numbers and the next does not reads as a bug."""
        fails_on_main = _conviction(escapes.FAILS_ON_MAIN, runs_before=88, failed_before=6,
                                    runs_after=99, failed_after=14)
        self.assertEqual(_emphasised(escapes.sentence(fails_on_main)),
                         ['14 of 99', 'after', '6 of 88'])
        self.assertEqual(_emphasised(escapes.sentence(_conviction(escapes.ESCAPED))),
                         ['2 of 6', '4'])
        self.assertEqual(_emphasised(escapes.sentence(_conviction(escapes.CONTAINED,
                                                                  failed_after=0))),
                         ['6 times'])
        self.assertEqual(_emphasised(escapes.sentence(_conviction(escapes.NO_BASELINE,
                                                                  runs_before=0))),
                         ['2 of 6'])
        self.assertEqual(_emphasised(escapes.sentence(_conviction(escapes.TREE_DIVERGED))),
                         ['3 times', '2 heads'])

    def test_a_verdict_with_no_counts_to_report_emphasises_nothing(self) -> None:
        self.assertEqual(_emphasised(escapes.sentence(_conviction(escapes.NO_RUNS, runs_after=0,
                                                                  failed_after=0))), [])


class TestConvictions(fixtures.DatabaseTest):
    """The individual convictions behind one verdict's count."""

    def _convict(self, number: int, test_name: str, verdict: str, pr_id: int,
                 builder: str = fixtures.LAYOUT_BUILDER, builder_id: int = 7,
                 landed_at: Optional[int] = LANDED_AT) -> int:
        build_id = self.store_build(number, flaky={test_name: config.CLEAN_TREE}, pr_id=pr_id,
                                    pr_title='A change that landed', builder=builder,
                                    builder_id=builder_id, sha='a' * 40)
        with self.connection:
            self.connection.execute(
                '''INSERT INTO escape_verdicts (
                    build_id, test_name, verdict, runs_before, failed_before, runs_after,
                    failed_after, landed_at, window_ends_at, decided_at
                ) VALUES (?,?,?,?,?,?,?,?,?,?)''',
                (build_id, test_name, verdict, 4, 0, 6, 2, landed_at,
                 LANDED_AT + escapes.ESCAPE_WINDOW_SECONDS, LANDED_AT),
            )
        return build_id

    def _convictions(self, verdict: str, **scope: object) -> list:
        return escapes.convictions(self.connection, fixtures.DEFAULT_BUILD_TIME - DAY,
                                   fixtures.DEFAULT_BUILD_TIME + DAY, verdict, **scope)

    def test_only_the_convictions_with_the_asked_for_verdict_are_listed(self) -> None:
        self._convict(1, TEST, escapes.CONTAINED, pr_id=1)
        self._convict(2, 'fast/b.html', escapes.NO_RUNS, pr_id=2)
        listed = self._convictions(escapes.CONTAINED)
        self.assertEqual([(one.test_name, one.verdict) for one in listed],
                         [(TEST, escapes.CONTAINED)])
        self.assertEqual(listed[0].rule, config.CLEAN_TREE)
        self.assertEqual(listed[0].landed_at, LANDED_AT)

    def test_a_queue_the_page_is_narrowed_to_narrows_the_list_too(self) -> None:
        self._convict(1, TEST, escapes.CONTAINED, pr_id=1)
        self._convict(2, 'fast/b.html', escapes.CONTAINED, pr_id=2,
                      builder=fixtures.GTK_BUILDER, builder_id=9)
        self.assertEqual(
            [one.test_name for one in self._convictions(escapes.CONTAINED,
                                                        builders=(fixtures.GTK_BUILDER,))],
            ['fast/b.html'],
        )

    def test_a_suite_the_page_is_narrowed_to_narrows_the_list_too(self) -> None:
        self._convict(1, TEST, escapes.CONTAINED, pr_id=1)
        self._convict(2, 'TestWebKitAPI.A.b', escapes.CONTAINED, pr_id=2,
                      builder=fixtures.API_BUILDER, builder_id=8)
        self.assertEqual([one.test_name for one in self._convictions(escapes.CONTAINED,
                                                                     suite='api-tests')],
                         ['TestWebKitAPI.A.b'])

    def test_the_heads_of_the_whole_pull_request_are_carried_not_this_build_s(self) -> None:
        """TREE_DIVERGED's sentence is about how far the pull request moved, which one build cannot
        say."""
        self._convict(1, TEST, escapes.TREE_DIVERGED, pr_id=PULL_REQUEST)
        self.store_build(2, flaky={TEST: config.CLEAN_TREE}, pr_id=PULL_REQUEST,
                         pr_title='A change that landed', sha='b' * 40,
                         started_at=fixtures.DEFAULT_BUILD_TIME + 600)
        listed = self._convictions(escapes.TREE_DIVERGED)
        self.assertEqual((listed[0].tested_sha, listed[0].newest_sha), ('a' * 40, 'b' * 40))
        self.assertEqual((listed[0].heads, listed[0].builds), (2, 2))

    def test_the_head_named_as_the_one_that_landed_predates_the_landing(self) -> None:
        """The listing has to name the head the verdict was reached against, or a reader is told a
        conviction diverged from a build EWS only started once the change was already on main."""
        self._convict(1, TEST, escapes.TREE_DIVERGED, pr_id=PULL_REQUEST)
        self.store_build(2, flaky={TEST: config.CLEAN_TREE}, pr_id=PULL_REQUEST,
                         pr_title='A change that landed', sha='b' * 40,
                         started_at=LANDED_AT + 600)
        listed = self._convictions(escapes.TREE_DIVERGED)
        self.assertEqual((listed[0].tested_sha, listed[0].newest_sha), ('a' * 40, 'a' * 40))

    def test_a_row_with_no_landing_time_is_still_listed_and_reports_none(self) -> None:
        """The backfill leaves a row whose landing the database no longer holds at null, and the
        counts either side of that landing are still the evidence the page exists to show, so the row
        stays and the missing time is reported as missing."""
        self._convict(1, TEST, escapes.ESCAPED, pr_id=1, landed_at=None)
        listed = self._convictions(escapes.ESCAPED)
        self.assertEqual([one.test_name for one in listed], [TEST])
        self.assertIsNone(listed[0].landed_at)
        self.assertEqual((listed[0].runs_after, listed[0].failed_after), (6, 2))

    def test_strength_and_damage_read_through_the_stored_counts(self) -> None:
        build_id = self._convict(1, TEST, escapes.ESCAPED, pr_id=1)
        with self.connection:
            self.connection.execute(
                'UPDATE escape_verdicts SET recent_runs = ?, recent_failed = ?, '
                'recent_checked_at = ? WHERE build_id = ? AND test_name = ?',
                (40, 3, LANDED_AT, build_id, TEST),
            )
        listed = self._convictions(escapes.ESCAPED)
        self.assertAlmostEqual(listed[0].strength, 0.11727270780688966, places=9)
        self.assertEqual(listed[0].damage, 0.075)


class TestStoredLandingTime(fixtures.DatabaseTest):
    """The landing time a verdict was decided about, which the row carries rather than derives.

    `escapes.ESCAPE_WINDOW_SECONDS` is computed at import time from `config.ESCAPE_WINDOW_DAYS`, so
    patching the config value would leave the module's constant alone and prove nothing; the constant
    itself is what a running dashboard would have been restarted with, and what these patch.
    """

    def _convict(self) -> int:
        return self.store_build(1, flaky={TEST: config.CLEAN_TREE}, pr_id=PULL_REQUEST,
                                pr_title='A change that landed', sha='a' * 40)

    def _listed(self, verdict: str) -> escapes.Conviction:
        listed = escapes.convictions(self.connection, fixtures.DEFAULT_BUILD_TIME - DAY,
                                     fixtures.DEFAULT_BUILD_TIME + DAY, verdict)
        self.assertEqual(len(listed), 1)
        return listed[0]

    def test_a_stored_landing_time_does_not_move_when_the_window_widens(self) -> None:
        """The whole point of storing it: the window's width is configuration, and back-deriving the
        landing from `window_ends_at` printed a date that never happened the moment it changed."""
        self._convict()
        self.store_landing(PULL_REQUEST, landed_at=LANDED_AT)
        history = fixtures.StubRunHistory({TEST: [fixtures.run(commit_at=LANDED_AT)]})
        escapes.assess(self.connection, history, fixtures.DEFAULT_BUILD_TIME - DAY,
                       fixtures.DEFAULT_BUILD_TIME + DAY)

        with mock.patch.object(escapes, 'ESCAPE_WINDOW_SECONDS', 30 * 86400):
            self.assertEqual(self._listed(escapes.CONTAINED).landed_at, LANDED_AT)

    def test_the_counts_stay_attached_to_the_window_they_were_counted_over(self) -> None:
        """The other half of storing both: a widened window must not claim the old counts came from
        it, so the row's window end is the one it was decided under."""
        self._convict()
        self.store_landing(PULL_REQUEST, landed_at=LANDED_AT)
        history = fixtures.StubRunHistory({TEST: [fixtures.run(commit_at=LANDED_AT)]})
        escapes.assess(self.connection, history, fixtures.DEFAULT_BUILD_TIME - DAY,
                       fixtures.DEFAULT_BUILD_TIME + DAY)
        counted_over = LANDED_AT + escapes.ESCAPE_WINDOW_SECONDS

        with mock.patch.object(escapes, 'ESCAPE_WINDOW_SECONDS', 30 * 86400):
            listed = self._listed(escapes.CONTAINED)
            self.assertEqual(listed.window_ends_at, counted_over)
            self.assertNotEqual(listed.window_ends_at, listed.landed_at + 30 * 86400)

    def _store_row(self, window_ends_at: int, decided_at: int, landed_at: int) -> None:
        with self.connection:
            self.connection.execute(
                '''INSERT INTO escape_verdicts (
                    build_id, test_name, verdict, runs_before, failed_before, runs_after,
                    failed_after, landed_at, window_ends_at, decided_at
                ) VALUES (?,?,?,?,?,?,?,?,?,?)''',
                (self._convict(), TEST, escapes.CONTAINED, 4, 0, 6, 0, landed_at, window_ends_at,
                 decided_at),
            )

    def _asked(self, history: fixtures.StubRunHistory) -> int:
        escapes.assess(self.connection, history, fixtures.DEFAULT_BUILD_TIME - DAY,
                       fixtures.DEFAULT_BUILD_TIME + DAY)
        return len(history.queries)

    def test_a_window_that_has_not_settled_is_asked_again_however_old_the_landing(self) -> None:
        """The settling check is about when the runs stop arriving, which is the end of the window and
        never the landing: keyed on the landing, this row would be kept a whole window early."""
        now = int(time.time())
        self.store_landing(PULL_REQUEST, landed_at=LANDED_AT)
        self._store_row(window_ends_at=now, decided_at=now, landed_at=LANDED_AT)

        self.assertGreater(self._asked(fixtures.StubRunHistory({TEST: []})), 0)

    def test_a_window_that_has_settled_is_kept_however_recent_the_landing(self) -> None:
        settled_end = fixtures.DEFAULT_BUILD_TIME
        self.store_landing(PULL_REQUEST, landed_at=int(time.time()) - 60)
        self._store_row(window_ends_at=settled_end,
                        decided_at=settled_end + results.RUNS_SETTLING_SECONDS,
                        landed_at=int(time.time()) - 60)

        self.assertEqual(self._asked(fixtures.StubRunHistory({TEST: []})), 0)
