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

"""Ingest reads whichever retry scheme a build actually ran, and explodes its flakiness map."""

from __future__ import annotations

import json
import socket
import urllib.error
from typing import Optional
from unittest import mock

from ews_dashboard import config, ingest, suites
from tests import fixtures


class TestFailureLists(fixtures.DatabaseTest):
    def _ingest(self, builder: str, extra_properties: dict, client: object = None) -> object:
        build = fixtures.build(extra_properties=extra_properties)
        ingest.ingest_build(
            self.connection, client or fixtures.StubBuildbot(), builder,
            builder_id=7, build=build,
        )
        return self.stored_build(build['buildid'])

    def test_classic_three_lists(self) -> None:
        stored = self._ingest(fixtures.LAYOUT_BUILDER, fixtures.failure_list_properties({
            'first_run_failures': ['fast/a.html', 'fast/b.html'],
            'second_run_failures': ['fast/a.html'],
            'clean_tree_run_failures': [],
        }))
        self.assertEqual(json.loads(stored['first_run_failures']), ['fast/a.html', 'fast/b.html'])
        self.assertEqual(json.loads(stored['second_run_failures']), ['fast/a.html'])
        self.assertEqual(json.loads(stored['clean_tree_run_failures']), [])
        self.assertEqual(stored['suite'], 'layout-tests')
        self.assertEqual(stored['flavor'], 'wk2')

    def test_filtered_lists_win_over_raw_lists(self) -> None:
        stored = self._ingest(fixtures.LAYOUT_BUILDER, fixtures.failure_list_properties({
            'first_run_failures': ['fast/a.html', 'fast/known-flake.html'],
            'second_run_failures': ['fast/a.html', 'fast/known-flake.html'],
            'first_run_failures_filtered': ['fast/a.html'],
            'second_run_failures_filtered': ['fast/a.html'],
            'clean_tree_run_failures': [],
        }))
        self.assertEqual(json.loads(stored['first_run_failures']), ['fast/a.html'])
        self.assertEqual(json.loads(stored['second_run_failures']), ['fast/a.html'])

    def test_first_run_filtered_alone_is_not_the_filtered_scheme(self) -> None:
        stored = self._ingest(fixtures.LAYOUT_BUILDER, fixtures.failure_list_properties({
            'first_run_failures': ['fast/a.html'],
            'first_run_failures_filtered': [],
            'clean_tree_run_failures': [],
        }))
        self.assertEqual(json.loads(stored['first_run_failures']), ['fast/a.html'])
        self.assertEqual(json.loads(stored['second_run_failures']), ['fast/a.html'])

    def test_repeat_failures_scheme(self) -> None:
        stored = self._ingest(fixtures.GTK_BUILDER, fixtures.failure_list_properties({
            'first_run_failures': ['imported/w3c/x.html'],
            'with_change_repeat_failures_results_nonflaky_failures': ['imported/w3c/x.html'],
            'without_change_repeat_failures_results_nonflaky_failures': [],
        }))
        self.assertEqual(json.loads(stored['second_run_failures']), ['imported/w3c/x.html'])
        self.assertEqual(json.loads(stored['clean_tree_run_failures']), [])

    def test_single_run_scheme_confirms_itself(self) -> None:
        stored = self._ingest(fixtures.LAYOUT_BUILDER, fixtures.failure_list_properties({
            'first_run_failures': ['http/tests/y.html'],
            'results-db_first_run_pre_existing': ['http/tests/pre.html'],
        }))
        self.assertEqual(json.loads(stored['first_run_failures']), ['http/tests/y.html'])
        self.assertEqual(json.loads(stored['second_run_failures']), ['http/tests/y.html'])
        self.assertEqual(json.loads(stored['clean_tree_run_failures']), ['http/tests/pre.html'])

    def test_a_build_with_no_failure_properties_stores_nothing_it_did_not_see(self) -> None:
        stored = self._ingest(fixtures.LAYOUT_BUILDER, {})
        self.assertIsNone(stored['first_run_failures'])
        self.assertIsNone(stored['second_run_failures'])
        self.assertIsNone(stored['clean_tree_run_failures'])

    def test_api_tests_come_from_the_retry_step_logs(self) -> None:
        client = fixtures.StubBuildbot(
            steps_by_build={1001: [
                {'name': 'run-api-tests', 'stepid': 11},
                {'name': 're-run-api-tests', 'stepid': 12},
                {'name': 'run-api-tests-without-change', 'stepid': 13},
            ]},
            logs={
                11: json.dumps({'Failed': [{'name': 'TestWebKitAPI.Foo'}],
                                'Timedout': [{'name': 'TestWebKitAPI.Slow'}], 'Crashed': []}),
                12: json.dumps({'Failed': [{'name': 'TestWebKitAPI.Foo'}]}),
                13: json.dumps({'Failed': []}),
            },
        )
        stored = self._ingest(fixtures.API_BUILDER, {}, client=client)
        self.assertEqual(json.loads(stored['first_run_failures']),
                         ['TestWebKitAPI.Foo', 'TestWebKitAPI.Slow'])
        self.assertEqual(json.loads(stored['second_run_failures']), ['TestWebKitAPI.Foo'])
        self.assertEqual(json.loads(stored['clean_tree_run_failures']), [])
        self.assertEqual(stored['flavor'], '')

    def test_api_tests_with_filtered_properties_skip_the_logs(self) -> None:
        client = fixtures.StubBuildbot()
        stored = self._ingest(fixtures.API_BUILDER, fixtures.failure_list_properties({
            'first_run_failures_filtered': ['TestWebKitAPI.Foo'],
            'second_run_failures_filtered': ['TestWebKitAPI.Foo'],
            'clean_tree_run_failures': [],
        }), client=client)
        self.assertEqual(json.loads(stored['first_run_failures']), ['TestWebKitAPI.Foo'])
        self.assertEqual(client.requested_logs, [])


class TestFlavor(fixtures.DatabaseTest):
    def _flavor(self, builder: str, extra_properties: dict) -> str:
        build = fixtures.build(extra_properties=extra_properties)
        ingest.ingest_build(
            self.connection, fixtures.StubBuildbot(), builder, builder_id=7, build=build,
        )
        return self.stored_build(build['buildid'])['flavor']

    def test_the_builds_own_property_beats_the_builder_name(self) -> None:
        self.assertEqual(
            self._flavor(fixtures.LAYOUT_BUILDER, fixtures.properties({'flavor': 'site-isolation'})),
            'site-isolation',
        )

    def test_a_flavor_ews_would_never_query_with_falls_back_to_the_builder_name(self) -> None:
        self.assertEqual(self._flavor(fixtures.LAYOUT_BUILDER,
                                      fixtures.properties({'flavor': 'jsc'})), 'wk2')

    def test_api_tests_have_no_flavor_whatever_the_build_reports(self) -> None:
        self.assertEqual(self._flavor(fixtures.API_BUILDER,
                                      fixtures.properties({'flavor': 'wk2'})), '')


class TestBuilderWalk(fixtures.DatabaseTest):
    def _walk(self, client: object) -> ingest.IngestReport:
        return ingest.ingest_builder(self.connection, client, fixtures.LAYOUT_BUILDER)

    def test_a_dropped_page_keeps_the_builds_already_read_and_records_the_failure(self) -> None:
        client = fixtures.HalfDeadBuildbot([fixtures.build(number=1)],
                                           urllib.error.URLError('reset'))
        report = self._walk(client)
        self.assertEqual(report.outcomes['ingested'], 1)
        self.assertEqual(report.failed, 1)
        self.assertIn('URLError', str(report.errors[0]))
        self.assertIn(fixtures.LAYOUT_BUILDER, str(report.errors[0]))

    def test_a_builder_that_cannot_even_be_looked_up_is_recorded_rather_than_raised(self) -> None:
        client = fixtures.HalfDeadBuildbot([], ValueError('no such builder'), error_on_listing=True)
        report = self._walk(client)
        self.assertEqual(report.outcomes['builder_failed'], 1)
        self.assertEqual(report.failed, 1)

    def test_an_error_names_the_builder_and_the_build_it_came_from(self) -> None:
        error = ingest.IngestError.of(fixtures.LAYOUT_BUILDER, 42, KeyError('suite'))
        self.assertEqual(str(error), f"{fixtures.LAYOUT_BUILDER} build 42: KeyError: 'suite'")

    def test_a_run_of_builds_already_held_ends_the_walk(self) -> None:
        for number in (5, 4):
            self.store_build(number)
        client = fixtures.WalkableBuildbot(
            [fixtures.build(number=number) for number in (5, 4, 3, 2, 1)])
        with mock.patch.object(ingest, 'SETTLED_RUN', 2):
            report = self._walk(client)
        self.assertEqual(report.outcomes['already_present'], 2)
        self.assertEqual(report.outcomes['ingested'], 0)

    def test_one_build_already_held_does_not_end_the_walk(self) -> None:
        """Buildbot numbers do not complete in order, so a straggler can sit below a build already
        held and would never be ingested if a single hit stopped the walk."""
        self.store_build(5)
        client = fixtures.WalkableBuildbot(
            [fixtures.build(number=number) for number in (5, 4)])
        report = self._walk(client)
        self.assertEqual(report.outcomes['already_present'], 1)
        self.assertEqual(report.outcomes['ingested'], 1)

    def test_a_page_of_api_tests_builds_reads_each_build_s_logs_once(self) -> None:
        builds = [fixtures.build(number=number) for number in (2, 1)]
        client = fixtures.WalkableBuildbot(
            builds,
            steps_by_build={build['buildid']: [{'name': 'run-api-tests',
                                                'stepid': build['buildid']}]
                            for build in builds},
            logs={build['buildid']: json.dumps({'Failed': [{'name': 'TestWebKitAPI.Foo'}]})
                  for build in builds},
        )
        report = ingest.ingest_builder(self.connection, client, fixtures.API_BUILDER)
        self.assertEqual(report.outcomes['ingested'], 2)
        self.assertEqual(len(client.requested_logs), len(builds))
        self.assertEqual(json.loads(self.stored_build(builds[0]['buildid'])['first_run_failures']),
                         ['TestWebKitAPI.Foo'])

    def test_a_build_whose_logs_cannot_be_read_is_recorded_without_ending_the_page(self) -> None:
        builds = [fixtures.build(number=number) for number in (2, 1)]
        readable = builds[1]['buildid']

        def lists(client: object, build_id: int, suite: object) -> ingest.FailureLists:
            if build_id != readable:
                raise urllib.error.URLError('reset')
            return ingest.FailureLists([], [], [])

        with mock.patch.object(ingest, '_lists_from_retry_step_logs', side_effect=lists):
            report = ingest.ingest_builder(self.connection, fixtures.WalkableBuildbot(builds),
                                           fixtures.API_BUILDER)
        self.assertEqual(report.outcomes['ingested'], 1)
        self.assertEqual(report.failed, 1)
        self.assertIn('build 2', str(report.errors[0]))

    def test_a_scrape_that_times_out_is_recorded_rather_than_ending_the_refresh(self) -> None:
        """A timed-out read reaches this path as a bare `socket.timeout`, which on Python 3.9 is
        neither a TimeoutError nor a URLError, and took a whole 90-day refresh down with it."""
        builds = [fixtures.build(number=number) for number in (2, 1)]
        readable = builds[1]['buildid']

        def lists(client: object, build_id: int, suite: object) -> ingest.FailureLists:
            if build_id != readable:
                raise socket.timeout('The read operation timed out')
            return ingest.FailureLists([], [], [])

        with mock.patch.object(ingest, '_lists_from_retry_step_logs', side_effect=lists):
            report = ingest.ingest_builder(self.connection, fixtures.WalkableBuildbot(builds),
                                           fixtures.API_BUILDER)
        self.assertEqual(report.outcomes['ingested'], 1)
        self.assertEqual(report.failed, 1)
        self.assertIn('build 2', str(report.errors[0]))


class TestWalkCoverage(fixtures.DatabaseTest):
    """A run of builds already held only means the walk has caught up inside a window some earlier
    walk went all the way through. Otherwise widening the window could never backfill history."""

    WEEK = fixtures.DEFAULT_BUILD_TIME - 7 * 86400
    FORTNIGHT = fixtures.DEFAULT_BUILD_TIME - 14 * 86400
    TWO_MONTHS = fixtures.DEFAULT_BUILD_TIME - 60 * 86400

    def setUp(self) -> None:
        super().setUp()
        # Only build 5 falls inside a week, both held builds inside a fortnight, everything inside
        # two months, so each window stops the walk somewhere different.
        self.held = [fixtures.build(number=5),
                     fixtures.build(number=4, started_at=fixtures.DEFAULT_BUILD_TIME - 10 * 86400)]
        self.old = [fixtures.build(number=number,
                                   started_at=fixtures.DEFAULT_BUILD_TIME - 30 * 86400)
                    for number in (2, 1)]
        # Buildbot completes builds out of order, so a build can appear below ones already held.
        self.straggler = fixtures.build(number=3,
                                        started_at=fixtures.DEFAULT_BUILD_TIME - 10 * 86400)

    def _walk(self, builds: list, since: int, limit: Optional[int] = None) -> ingest.IngestReport:
        return ingest.ingest_builder(self.connection, fixtures.WalkableBuildbot(builds),
                                     fixtures.LAYOUT_BUILDER, since=since, limit=limit)

    def test_a_wider_window_walks_past_the_run_a_narrower_one_stopped_on(self) -> None:
        everything = self.held + [self.straggler] + self.old
        with mock.patch.object(ingest, 'SETTLED_RUN', 2):
            self._walk(self.held, since=self.FORTNIGHT)
            settled = self._walk(everything, since=self.FORTNIGHT)
            deeper = self._walk(everything, since=self.TWO_MONTHS)
        self.assertEqual(settled.outcomes['ingested'], 0)
        self.assertEqual(deeper.outcomes['ingested'], 3)

    def test_a_narrower_walk_does_not_shrink_the_reach_a_wider_one_earned(self) -> None:
        """The second walk runs to the end of its own week rather than stopping early, so it is the
        one that would overwrite the reach it inherited."""
        with mock.patch.object(ingest, 'SETTLED_RUN', 2):
            self._walk(self.held + self.old, since=self.TWO_MONTHS)
            self._walk(self.held + self.old, since=self.WEEK)
            settled = self._walk(self.held + [self.straggler] + self.old, since=self.TWO_MONTHS)
        self.assertEqual(settled.outcomes['ingested'], 0)

    def test_a_walk_that_died_claims_none_of_the_window_it_never_finished(self) -> None:
        with mock.patch.object(ingest, 'SETTLED_RUN', 2):
            died = ingest.ingest_builder(
                self.connection,
                fixtures.HalfDeadBuildbot(self.held, urllib.error.URLError('reset')),
                fixtures.LAYOUT_BUILDER, since=self.FORTNIGHT,
            )
            after = self._walk(self.held + [self.straggler], since=self.FORTNIGHT)
        self.assertEqual(died.failed, 1)
        self.assertEqual(after.outcomes['ingested'], 1)

    def test_a_walk_cut_short_by_a_limit_claims_nothing_either(self) -> None:
        with mock.patch.object(ingest, 'SETTLED_RUN', 2):
            self._walk(self.held + [self.straggler], since=self.FORTNIGHT, limit=2)
            after = self._walk(self.held + [self.straggler], since=self.FORTNIGHT)
        self.assertEqual(after.outcomes['ingested'], 1)


class TestFlakinessVerdicts(fixtures.DatabaseTest):
    def _ingest(self, extra_properties: dict, number: int = 1, force: bool = False,
                builder: str = fixtures.LAYOUT_BUILDER) -> dict:
        build = fixtures.build(number=number, extra_properties=extra_properties)
        outcome = ingest.ingest_build(
            self.connection, fixtures.StubBuildbot(), builder,
            builder_id=7, build=build, force=force,
        )
        return {'outcome': outcome, 'build_id': build['buildid']}

    def test_both_runs_are_kept(self) -> None:
        ingested = self._ingest(fixtures.properties({
            'results-db_first_run_flaky': json.dumps({'fast/a.html': config.CLEAN_TREE,
                                                      'fast/b.html': config.DIRTY_TREE}),
            'results-db_second_run_flaky': json.dumps({'fast/a.html': config.CLEAN_TREE}),
        }))
        self.assertEqual(
            [(row['run_number'], row['test_name'], row['rule'])
             for row in self.stored_verdicts(ingested['build_id'])],
            [(1, 'fast/a.html', config.CLEAN_TREE),
             (1, 'fast/b.html', config.DIRTY_TREE),
             (2, 'fast/a.html', config.CLEAN_TREE)],
        )

    def test_a_query_with_no_answer_is_recorded_as_a_failure_not_a_conviction(self) -> None:
        ingested = self._ingest(fixtures.properties({
            'results-db_second_run_flaky': json.dumps({'fast/a.html': config.CLEAN_TREE}),
            'results-db_second_run_flaky_unknown': json.dumps(['fast/a.html', 'fast/gone.html']),
        }))
        rows = {row['test_name']: row for row in self.stored_verdicts(ingested['build_id'])}
        self.assertEqual(rows['fast/a.html']['query_failed'], 0)
        self.assertEqual(rows['fast/gone.html']['query_failed'], 1)
        self.assertIsNone(rows['fast/gone.html']['rule'])

    def test_between_builds_records_whether_the_bot_had_within_build_evidence(self) -> None:
        ingested = self._ingest(fixtures.properties({
            'results-db_first_run_flaky': json.dumps({'fast/with.html': config.BETWEEN_BUILDS,
                                                      'fast/without.html': config.BETWEEN_BUILDS,
                                                      'fast/other.html': config.DIRTY_TREE}),
            'results-db_first_run_flaky_unsupported': json.dumps(['fast/without.html']),
        }))
        rows = {row['test_name']: row for row in self.stored_verdicts(ingested['build_id'])}
        self.assertEqual(rows['fast/with.html']['within_build_evidence'], 1)
        self.assertEqual(rows['fast/without.html']['within_build_evidence'], 0)
        self.assertIsNone(rows['fast/other.html']['within_build_evidence'])

    def test_a_build_that_asked_and_convicted_nothing_is_not_a_build_that_never_asked(self) -> None:
        asked = self._ingest(
            fixtures.properties({'results-db_second_run_flaky': json.dumps({})}), number=1,
        )
        never_asked = self._ingest({}, number=2)
        self.assertEqual(self.stored_build(asked['build_id'])['flakiness_query_ran'], 1)
        self.assertEqual(self.stored_verdicts(asked['build_id']), [])
        self.assertEqual(self.stored_build(never_asked['build_id'])['flakiness_query_ran'], 0)

    def test_a_second_ingest_is_a_no_op_without_force(self) -> None:
        first = self._ingest(fixtures.properties({
            'results-db_first_run_flaky': json.dumps({'fast/a.html': config.CLEAN_TREE}),
        }))
        again = self._ingest({})
        self.assertEqual(again['outcome'], 'already_present')
        self.assertEqual(len(self.stored_verdicts(first['build_id'])), 1)

    def test_force_replaces_the_verdicts_it_previously_stored(self) -> None:
        first = self._ingest(fixtures.properties({
            'results-db_first_run_flaky': json.dumps({'fast/a.html': config.CLEAN_TREE}),
        }))
        forced = self._ingest(fixtures.properties({
            'results-db_first_run_flaky': json.dumps({'fast/b.html': config.DIRTY_TREE}),
        }), force=True)
        self.assertEqual(forced['outcome'], 'reingested')
        self.assertEqual([row['test_name'] for row in self.stored_verdicts(first['build_id'])],
                         ['fast/b.html'])

    def test_a_reingest_keeps_the_escape_verdicts_it_cost_a_round_trip_to_decide(self) -> None:
        first = self._ingest(fixtures.properties({
            'results-db_first_run_flaky': json.dumps({'fast/a.html': config.CLEAN_TREE}),
        }))
        self._store_escape_verdict(first['build_id'], 'fast/a.html')
        self._ingest({}, force=True)
        self.assertEqual(self._stored_escape_verdicts(first['build_id']), ['fast/a.html'])

    def test_a_reingest_updates_the_build_row(self) -> None:
        first = self._ingest({})
        self._ingest(fixtures.properties({'github.title': 'A retitled change'}), force=True)
        stored = self.stored_build(first['build_id'])
        self.assertEqual(stored['pr_title'], 'A retitled change')
        self.assertEqual(
            self.connection.execute('SELECT COUNT(*) FROM build_verdicts').fetchone()[0], 1)

    def _store_escape_verdict(self, build_id: int, test_name: str) -> None:
        with self.connection:
            self.connection.execute(
                '''INSERT INTO escape_verdicts (
                    build_id, test_name, verdict, window_ends_at, decided_at
                ) VALUES (?,?,?,?,?)''',
                (build_id, test_name, 'ESCAPED', fixtures.DEFAULT_BUILD_TIME,
                 fixtures.DEFAULT_BUILD_TIME),
            )

    def _stored_escape_verdicts(self, build_id: int) -> list:
        return [row['test_name'] for row in self.connection.execute(
            'SELECT test_name FROM escape_verdicts WHERE build_id = ? ORDER BY test_name',
            (build_id,),
        )]

    def test_api_flaky_map_is_stored_at_the_rerun_slot(self) -> None:
        """api-tests has one read, not two runs of its own; it happens at ReRunAPITests.doOnFailure,
        the rerun still carrying the change, so it lands at run_number 2 -- the same slot layout's
        own second run fills."""
        ingested = self._ingest(fixtures.properties({
            'results-db_api_flaky': json.dumps({
                'TestWebKitAPI.DownloadProgress.ExtraData': config.BETWEEN_BUILDS,
            }),
        }), number=1, builder=fixtures.API_BUILDER)
        self.assertEqual(
            [(row['run_number'], row['test_name'], row['rule'])
             for row in self.stored_verdicts(ingested['build_id'])],
            [(2, 'TestWebKitAPI.DownloadProgress.ExtraData', config.BETWEEN_BUILDS)],
        )

    def test_api_flaky_unsupported_records_missing_within_build_evidence(self) -> None:
        ingested = self._ingest(fixtures.properties({
            'results-db_api_flaky': json.dumps({
                'TestWebKitAPI.DownloadProgress.ExtraData': config.BETWEEN_BUILDS,
                'TestWebKitAPI.ProcessSwap.NavigateBackAfterCrash': config.BETWEEN_BUILDS,
            }),
            'results-db_api_flaky_unsupported': json.dumps(
                ['TestWebKitAPI.ProcessSwap.NavigateBackAfterCrash']),
        }), number=1, builder=fixtures.API_BUILDER)
        rows = {row['test_name']: row for row in self.stored_verdicts(ingested['build_id'])}
        self.assertEqual(
            rows['TestWebKitAPI.DownloadProgress.ExtraData']['within_build_evidence'], 1)
        self.assertEqual(
            rows['TestWebKitAPI.ProcessSwap.NavigateBackAfterCrash']['within_build_evidence'], 0)

    def test_api_flaky_unknown_is_a_query_failure_not_a_conviction(self) -> None:
        ingested = self._ingest(fixtures.properties({
            'results-db_api_flaky': json.dumps({
                'TestWebKitAPI.DownloadProgress.ExtraData': config.CLEAN_TREE,
            }),
            'results-db_api_flaky_unknown': json.dumps(
                ['TestWebKitAPI.DownloadProgress.ExtraData', 'TestWebKitAPI.Gone.Test']),
        }), number=1, builder=fixtures.API_BUILDER)
        rows = {row['test_name']: row for row in self.stored_verdicts(ingested['build_id'])}
        self.assertEqual(rows['TestWebKitAPI.DownloadProgress.ExtraData']['query_failed'], 0)
        self.assertEqual(rows['TestWebKitAPI.Gone.Test']['query_failed'], 1)
        self.assertIsNone(rows['TestWebKitAPI.Gone.Test']['rule'])

    def test_api_flaky_alone_marks_the_query_as_having_run(self) -> None:
        asked = self._ingest(
            fixtures.properties({'results-db_api_flaky': json.dumps({})}), number=1,
            builder=fixtures.API_BUILDER)
        never_asked = self._ingest({}, number=2, builder=fixtures.API_BUILDER)
        self.assertEqual(self.stored_build(asked['build_id'])['flakiness_query_ran'], 1)
        self.assertEqual(self.stored_build(never_asked['build_id'])['flakiness_query_ran'], 0)

    def test_an_incomplete_build_is_skipped(self) -> None:
        build = fixtures.build(complete=False)
        outcome = ingest.ingest_build(
            self.connection, fixtures.StubBuildbot(), fixtures.LAYOUT_BUILDER,
            builder_id=7, build=build,
        )
        self.assertEqual(outcome, 'skipped')
        self.assertIsNone(self.stored_build(build['buildid']))


class TestBuilderRecognition(fixtures.DatabaseTest):
    def test_builders_this_dashboard_cannot_read(self) -> None:
        for builder in ('JSC-Tests-x86-64-EWS', 'Bindings-Tests-EWS', 'WebKitPy-Tests-EWS',
                        'WebKitPerl-Tests-EWS', 'macOS-Sequoia-Release-Build-EWS'):
            self.assertIsNone(suites.suite_for_builder(builder), builder)

    def test_an_embedded_api_tests_builder_is_not_a_layout_tests_builder(self) -> None:
        suite = suites.suite_for_builder(fixtures.API_BUILDER)
        self.assertIsNotNone(suite)
        self.assertEqual(suite.name, 'api-tests')

    def test_gtk_and_wpe_platforms_are_capitalized_for_results_webkit_org(self) -> None:
        self.assertEqual(ingest.platform_of('gtk'), 'GTK')
        self.assertEqual(ingest.platform_of('wpe'), 'WPE')
        self.assertEqual(ingest.platform_of('mac'), 'mac')
        self.assertIsNone(ingest.platform_of(None))
