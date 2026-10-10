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

"""Convictions are counted per build and test, from the answer that stood for each test."""

from __future__ import annotations

import inspect

from ews_dashboard import config, suites
from ews_dashboard.analysis import convictions, filters
from tests import fixtures

WINDOW = (fixtures.DEFAULT_BUILD_TIME - 86400, fixtures.DEFAULT_BUILD_TIME + 86400)


def order(sort: str, descending: bool) -> tuple:
    """The sort keys the explore page builds for one header link."""
    return filters.sort_keys(filters.TESTS, ((sort, descending),))


def named(fragment: str) -> tuple:
    return filters.parse(filters.TESTS, (('test', 'has', fragment),))


class TestByRule(fixtures.DatabaseTest):
    def test_a_rule_that_never_fired_reports_zero_rather_than_being_absent(self) -> None:
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE})
        counted = convictions.by_rule(self.connection, *WINDOW)
        self.assertEqual(set(counted), set(config.FLAKINESS_RULES))
        self.assertEqual(counted[config.CLEAN_TREE], 1)
        self.assertEqual(counted[config.BETWEEN_BUILDS], 0)

    def test_the_same_test_convicted_in_two_builds_counts_twice(self) -> None:
        self.store_build(1, flaky={'fast/a.html': config.DIRTY_TREE})
        self.store_build(2, flaky={'fast/a.html': config.DIRTY_TREE})
        self.assertEqual(convictions.by_rule(self.connection, *WINDOW)[config.DIRTY_TREE], 2)

    def test_a_query_with_no_answer_is_not_a_conviction(self) -> None:
        self.store_build(1, query_failed=['fast/a.html'])
        self.assertEqual(sum(convictions.by_rule(self.connection, *WINDOW).values()), 0)
        self.assertEqual(convictions.query_failures(self.connection, *WINDOW), 1)

    def test_builds_outside_the_window_are_not_counted(self) -> None:
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE},
                         started_at=WINDOW[0] - 86400)
        self.assertEqual(sum(convictions.by_rule(self.connection, *WINDOW).values()), 0)

    def test_the_rerun_answer_stands_where_it_disagrees_with_the_first_run(self) -> None:
        self.store_build(
            1,
            flaky_first_run={'fast/a.html': config.CLEAN_TREE, 'fast/only-first.html': config.DIRTY_TREE},
            flaky={'fast/a.html': config.DIRTY_TREE},
        )
        counted = convictions.by_rule(self.connection, *WINDOW)
        self.assertEqual(counted[config.CLEAN_TREE], 0)
        self.assertEqual(counted[config.DIRTY_TREE], 2)


class TestBuildsQueried(fixtures.DatabaseTest):
    def test_a_build_that_asked_and_convicted_nothing_still_counts_as_having_asked(self) -> None:
        self.store_build(1, flaky={})
        self.store_build(2)
        self.assertEqual(convictions.builds_queried(self.connection, *WINDOW), 1)


class TestScopedCounts(fixtures.DatabaseTest):
    """The landing page's cards narrow the same way its rule table and the explore page do, so a
    reader who picks one queue is not shown a rate computed over every queue."""

    def setUp(self) -> None:
        super().setUp()
        self.store_build(1, flaky={'fast/layout.html': config.CLEAN_TREE})
        self.store_build(2, flaky={'fast/api.html': config.CLEAN_TREE},
                         query_failed=['fast/unanswered.html'],
                         builder=fixtures.API_BUILDER, builder_id=9)

    def test_convictions_narrow_to_one_queue(self) -> None:
        counted = convictions.by_rule(self.connection, *WINDOW, builders=(fixtures.API_BUILDER,))
        self.assertEqual(counted[config.CLEAN_TREE], 1)
        self.assertEqual(sum(convictions.by_rule(self.connection, *WINDOW).values()), 2)

    def test_convictions_narrow_to_one_suite(self) -> None:
        suite = suites.suite_for_builder(fixtures.API_BUILDER).name
        counted = convictions.by_rule(self.connection, *WINDOW, suite=suite)
        self.assertEqual(counted[config.CLEAN_TREE], 1)

    def test_builds_that_asked_narrow_to_one_queue(self) -> None:
        self.assertEqual(convictions.builds_queried(self.connection, *WINDOW), 2)
        self.assertEqual(
            convictions.builds_queried(self.connection, *WINDOW, builders=(fixtures.API_BUILDER,)), 1)

    def test_unanswered_queries_narrow_to_one_queue(self) -> None:
        self.assertEqual(
            convictions.query_failures(self.connection, *WINDOW, builders=(fixtures.API_BUILDER,)), 1)
        self.assertEqual(
            convictions.query_failures(self.connection, *WINDOW,
                                       builders=(fixtures.LAYOUT_BUILDER,)), 0)


class TestConvictedTests(fixtures.DatabaseTest):
    def setUp(self) -> None:
        super().setUp()
        self.store_build(1, flaky={'fast/often.html': config.CLEAN_TREE},
                         started_at=fixtures.DEFAULT_BUILD_TIME)
        self.store_build(2, flaky={'fast/often.html': config.CLEAN_TREE},
                         started_at=fixtures.DEFAULT_BUILD_TIME + 600)
        self.store_build(3, flaky={'fast/rare.html': config.CLEAN_TREE},
                         builder=fixtures.API_BUILDER, builder_id=9)

    def _convicted(self, **narrowing: object) -> list:
        return convictions.convicted_tests(self.connection, *WINDOW,
                                           sort_keys=order('convictions', True),
                                           **narrowing).tests

    def test_most_convicted_first(self) -> None:
        self.assertEqual([test.test_name for test in self._convicted()],
                         ['fast/often.html', 'fast/rare.html'])
        self.assertEqual(self._convicted()[0].convictions, 2)

    def test_the_linked_build_is_the_most_recent_conviction(self) -> None:
        self.assertEqual(self._convicted()[0].build_number, 2)
        self.assertEqual(self._convicted()[0].last_seen, fixtures.DEFAULT_BUILD_TIME + 600)

    def test_the_configuration_travels_with_the_test_so_it_can_be_linked(self) -> None:
        configuration = self._convicted()[0].configuration
        self.assertEqual((configuration.suite, configuration.platform, configuration.style),
                         ('layout-tests', 'mac', 'release'))
        self.assertEqual(configuration.flavor, 'wk2')

    def test_a_builder_filter_narrows_the_list(self) -> None:
        self.assertEqual([test.test_name for test in self._convicted(builders=(fixtures.API_BUILDER,))],
                         ['fast/rare.html'])

    def test_a_suite_filter_narrows_the_list(self) -> None:
        self.assertEqual([test.test_name for test in self._convicted(suite='api-tests')],
                         ['fast/rare.html'])

    def test_the_limit_is_honoured(self) -> None:
        self.assertEqual(len(self._convicted(limit=1)), 1)

    def test_a_test_convicted_on_two_queues_reports_two_queues(self) -> None:
        self.store_build(4, flaky={'fast/rare.html': config.CLEAN_TREE})
        by_name = {test.test_name: test for test in self._convicted()}
        self.assertEqual(by_name['fast/rare.html'].queues, 2)


class TestConvictedTestsAcrossRules(fixtures.DatabaseTest):
    """One row per test whatever rules fired against it, so a reader is never asked to open three
    tables to find one test."""

    def test_no_rule_given_reports_every_rule_that_fired(self) -> None:
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE})
        self.store_build(2, flaky={'fast/b.html': config.DIRTY_TREE})
        convicted = convictions.convicted_tests(self.connection, *WINDOW,
                                                sort_keys=order('test', False)).tests
        self.assertEqual([(test.test_name, test.rules) for test in convicted],
                         [('fast/a.html', (config.CLEAN_TREE,)), ('fast/b.html', (config.DIRTY_TREE,))])

    def test_a_query_failure_is_never_counted_as_a_conviction_under_any_rule(self) -> None:
        self.store_build(1, query_failed=['fast/unanswered.html'])
        self.assertEqual(convictions.convicted_tests(self.connection, *WINDOW).total, 0)

    def test_a_test_convicted_under_two_rules_is_one_row_whose_rules_holds_both(self) -> None:
        self.store_build(1, flaky={'fast/both.html': config.CLEAN_TREE})
        self.store_build(2, flaky={'fast/both.html': config.DIRTY_TREE})
        [convicted] = convictions.convicted_tests(self.connection, *WINDOW).tests
        self.assertEqual(convicted.test_name, 'fast/both.html')
        self.assertEqual(convicted.rules, ('CleanTree', 'DirtyTree'))
        self.assertEqual(convicted.convictions, 2)

    def test_a_rule_filter_narrows_the_test_set_without_shrinking_a_kept_tests_own_group(self) -> None:
        """`anyof` narrows which tests are in the answer, not what a kept test's own row reports:
        `fast/both.html` was convicted under both rules, so it still reports both once it is kept, the
        wrong answer a `WHERE` on `verdict.rule` would have given being one row that only ever
        mentioned `CleanTree`."""
        self.store_build(1, flaky={'fast/both.html': config.CLEAN_TREE})
        self.store_build(2, flaky={'fast/both.html': config.DIRTY_TREE})
        self.store_build(3, flaky={'fast/dirty-only.html': config.DIRTY_TREE})
        conditions = filters.parse(filters.TESTS, (('rule', 'anyof', config.CLEAN_TREE),))
        [narrowed] = convictions.convicted_tests(self.connection, *WINDOW,
                                                 conditions=conditions).tests
        self.assertEqual(narrowed.test_name, 'fast/both.html')
        self.assertEqual(narrowed.rules, ('CleanTree', 'DirtyTree'))
        self.assertEqual(narrowed.convictions, 2)


class TestConvictedTestsSetOperators(fixtures.DatabaseTest):
    """The five ways a `rule` chip can ask about a test's whole set of flake types, told apart against
    three tests: one convicted under both rules, one under `CleanTree` alone, and one under
    `DirtyTree` alone."""

    BOTH = 'fast/both.html'
    CLEAN_ONLY = 'fast/clean-only.html'
    DIRTY_ONLY = 'fast/dirty-only.html'

    def setUp(self) -> None:
        super().setUp()
        self.store_build(1, flaky={self.BOTH: config.CLEAN_TREE})
        self.store_build(2, flaky={self.BOTH: config.DIRTY_TREE})
        self.store_build(3, flaky={self.CLEAN_ONLY: config.CLEAN_TREE})
        self.store_build(4, flaky={self.DIRTY_ONLY: config.DIRTY_TREE})

    def _kept(self, operator: str, value: str) -> list:
        conditions = filters.parse(filters.TESTS, (('rule', operator, value),))
        page = convictions.convicted_tests(self.connection, *WINDOW, conditions=conditions,
                                           sort_keys=order('test', False))
        return [test.test_name for test in page.tests]

    def test_anyof_keeps_every_test_with_at_least_one_matching_rule(self) -> None:
        self.assertEqual(self._kept('anyof', config.CLEAN_TREE), [self.BOTH, self.CLEAN_ONLY])

    def test_allof_keeps_only_the_test_convicted_under_every_named_rule(self) -> None:
        self.assertEqual(self._kept('allof', f'{config.CLEAN_TREE}|{config.DIRTY_TREE}'), [self.BOTH])

    def test_oneof_keeps_only_tests_convicted_under_exactly_one_named_rule(self) -> None:
        self.assertEqual(self._kept('oneof', f'{config.CLEAN_TREE}|{config.DIRTY_TREE}'),
                         [self.CLEAN_ONLY, self.DIRTY_ONLY])

    def test_exactly_keeps_only_the_test_whose_whole_rule_set_is_the_named_rules(self) -> None:
        self.assertEqual(self._kept('exactly', f'{config.CLEAN_TREE}|{config.DIRTY_TREE}'),
                         [self.BOTH])

    def test_noneof_keeps_every_test_with_no_matching_rule(self) -> None:
        self.assertEqual(self._kept('noneof', config.CLEAN_TREE), [self.DIRTY_ONLY])

    def test_a_row_kept_by_anyof_still_reports_every_rule_it_was_convicted_under(self) -> None:
        """The point of moving a set condition to HAVING: a `WHERE` on `verdict.rule` would have
        narrowed the group before `GROUP_CONCAT` ran, so `fast/both.html` would have reported only
        `CleanTree` even though it was also convicted under `DirtyTree`."""
        conditions = filters.parse(filters.TESTS, (('rule', 'anyof', config.CLEAN_TREE),))
        by_name = {test.test_name: test
                   for test in convictions.convicted_tests(self.connection, *WINDOW,
                                                           conditions=conditions).tests}
        self.assertEqual(by_name[self.BOTH].rules, ('CleanTree', 'DirtyTree'))
        self.assertEqual(by_name[self.BOTH].convictions, 2)

    def test_the_total_agrees_with_the_rows_kept_under_a_having_only_filter(self) -> None:
        """The count subquery and the capped query are built from the same `selection` string, so a
        condition that moved into HAVING still narrows both together; a total that disagreed would
        report a set as whole when the cap had actually cut it."""
        conditions = filters.parse(filters.TESTS, (('rule', 'anyof', config.CLEAN_TREE),))
        page = convictions.convicted_tests(self.connection, *WINDOW, conditions=conditions,
                                           limit=1, sort_keys=order('test', False))
        self.assertEqual(page.total, 2)
        self.assertTrue(page.truncated)
        self.assertEqual([test.test_name for test in page.tests], [self.BOTH])


class TestConvictedTestsCap(fixtures.DatabaseTest):
    """The set has a hard cap instead of a page, so the query's cost has a ceiling whatever
    day-window or filter a reader picks, and the caller learns whether the cap cut the set."""

    def _convict(self, names: list) -> None:
        for number, name in enumerate(names, start=1):
            self.store_build(number, flaky={name: config.CLEAN_TREE})

    def test_the_default_cap_is_a_thousand(self) -> None:
        self.assertEqual(inspect.signature(convictions.convicted_tests).parameters['limit'].default,
                         1000)

    def test_the_cap_bounds_the_rows_returned(self) -> None:
        self._convict([f'fast/flake{number:02d}.html' for number in range(5)])
        page = convictions.convicted_tests(self.connection, *WINDOW, limit=3)
        self.assertEqual(len(page.tests), 3)

    def test_more_rows_than_the_cap_reports_the_whole_total_and_is_truncated(self) -> None:
        self._convict([f'fast/flake{number:02d}.html' for number in range(5)])
        page = convictions.convicted_tests(self.connection, *WINDOW, limit=3)
        self.assertEqual(page.total, 5)
        self.assertTrue(page.truncated)

    def test_fewer_rows_than_the_cap_reports_the_same_total_and_is_not_truncated(self) -> None:
        self._convict(['fast/a.html', 'fast/b.html'])
        page = convictions.convicted_tests(self.connection, *WINDOW, limit=10)
        self.assertEqual(page.total, 2)
        self.assertFalse(page.truncated)

    def test_an_empty_result_is_not_truncated(self) -> None:
        page = convictions.convicted_tests(self.connection, *WINDOW)
        self.assertEqual((page.tests, page.total), ([], 0))
        self.assertFalse(page.truncated)


class TestConvictedTestsFiltering(fixtures.DatabaseTest):
    """A filter narrows the rows and the total together, whatever the total ends up being."""

    def _convict(self, names: list) -> None:
        for number, name in enumerate(names, start=1):
            self.store_build(number, flaky={name: config.CLEAN_TREE})

    def test_every_order_ends_in_a_column_no_two_tests_can_share(self) -> None:
        """What makes a capped set stable is the clause, not the engine: sqlite is free to break a
        tie differently between runs, and this fixture only happens not to, so the guarantee has to
        be asserted where it lives.
        """
        for key in filters.TESTS.sortable_names:
            for descending in (True, False):
                self.assertTrue(
                    filters.order_by(filters.TESTS, order(key, descending)).endswith(
                        'verdict.test_name ASC'),
                    f'{key} {descending} does not end in a total order')

    def test_the_total_is_the_same_whatever_the_cap_is_asked_for(self) -> None:
        self._convict(['fast/a.html', 'fast/b.html', 'fast/c.html'])
        self.assertEqual(convictions.convicted_tests(self.connection, *WINDOW, limit=1).total, 3)
        self.assertEqual(convictions.convicted_tests(self.connection, *WINDOW, limit=25).total, 3)

    def test_a_name_filter_narrows_the_rows_and_the_total_together(self) -> None:
        self._convict(['fast/editing-one.html', 'fast/editing-two.html', 'fast/other.html'])
        page = convictions.convicted_tests(self.connection, *WINDOW, conditions=named('editing'))
        self.assertEqual([test.test_name for test in page.tests],
                         ['fast/editing-one.html', 'fast/editing-two.html'])
        self.assertEqual(page.total, 2)
        self.assertFalse(page.truncated)

    def test_a_name_filter_that_matches_nothing_reports_an_empty_set(self) -> None:
        self._convict(['fast/a.html'])
        page = convictions.convicted_tests(self.connection, *WINDOW,
                                           conditions=named('nothing-like-this'))
        self.assertEqual((page.tests, page.total), ([], 0))

    def test_a_wildcard_a_reader_typed_matches_itself_rather_than_anything(self) -> None:
        self._convict(['fast/100%-scale.html', 'fast/plain.html'])
        page = convictions.convicted_tests(self.connection, *WINDOW, conditions=named('100%'))
        self.assertEqual([test.test_name for test in page.tests], ['fast/100%-scale.html'])
        wide = convictions.convicted_tests(self.connection, *WINDOW, conditions=named('%'))
        self.assertEqual(wide.total, 1)

    def test_an_underscore_a_reader_typed_matches_itself_rather_than_any_character(self) -> None:
        self._convict(['fast/a_b.html', 'fast/axb.html'])
        page = convictions.convicted_tests(self.connection, *WINDOW, conditions=named('a_b'))
        self.assertEqual([test.test_name for test in page.tests], ['fast/a_b.html'])

    def test_a_condition_on_an_aggregate_narrows_the_rows_and_the_total_together(self) -> None:
        """An aggregate is a HAVING, so the total has to be counted over the grouped rows: a total
        that counted the ungrouped ones would report a set as whole that the cap had actually cut."""
        for number, name in enumerate(['fast/once.html', 'fast/twice.html', 'fast/twice.html',
                                       'fast/thrice.html', 'fast/thrice.html', 'fast/thrice.html'],
                                      start=1):
            self.store_build(number, flaky={name: config.CLEAN_TREE})
        conditions = filters.parse(filters.TESTS, (('convictions', 'ge', '2'),))
        page = convictions.convicted_tests(self.connection, *WINDOW, conditions=conditions,
                                           sort_keys=order('convictions', True))
        self.assertEqual([test.test_name for test in page.tests],
                         ['fast/thrice.html', 'fast/twice.html'])
        unfiltered = convictions.convicted_tests(self.connection, *WINDOW)
        self.assertEqual((page.total, unfiltered.total), (2, 3))


class TestConvictedTestOrder(fixtures.DatabaseTest):
    """Every sort key orders these two tests differently, so a clause that ignored the key it was
    handed would still pass one assertion and fail the rest.

    `fast/aaa.html` is convicted three times across two queues and last seen first; `fast/zzz.html`
    once, on one queue, and last of all.
    """

    OFTEN = 'fast/aaa.html'
    RARE = 'fast/zzz.html'

    def setUp(self) -> None:
        super().setUp()
        self.store_build(1, flaky={self.OFTEN: config.CLEAN_TREE},
                         started_at=fixtures.DEFAULT_BUILD_TIME)
        self.store_build(2, flaky={self.OFTEN: config.CLEAN_TREE},
                         started_at=fixtures.DEFAULT_BUILD_TIME + 100)
        self.store_build(3, flaky={self.OFTEN: config.CLEAN_TREE},
                         started_at=fixtures.DEFAULT_BUILD_TIME + 200,
                         builder=fixtures.API_BUILDER, builder_id=9)
        self.store_build(4, flaky={self.RARE: config.CLEAN_TREE},
                         started_at=fixtures.DEFAULT_BUILD_TIME + 300)

    def _names(self, sort: str, descending: bool) -> list:
        convicted = convictions.convicted_tests(self.connection, *WINDOW,
                                                sort_keys=order(sort, descending))
        return [test.test_name for test in convicted.tests]

    def test_by_name(self) -> None:
        self.assertEqual(self._names('test', descending=False), [self.OFTEN, self.RARE])
        self.assertEqual(self._names('test', descending=True), [self.RARE, self.OFTEN])

    def test_by_convictions(self) -> None:
        self.assertEqual(self._names('convictions', descending=True), [self.OFTEN, self.RARE])
        self.assertEqual(self._names('convictions', descending=False), [self.RARE, self.OFTEN])

    def test_by_queues(self) -> None:
        self.assertEqual(self._names('queues', descending=True), [self.OFTEN, self.RARE])
        self.assertEqual(self._names('queues', descending=False), [self.RARE, self.OFTEN])

    def test_by_last_seen(self) -> None:
        self.assertEqual(self._names('last_seen', descending=True), [self.RARE, self.OFTEN])
        self.assertEqual(self._names('last_seen', descending=False), [self.OFTEN, self.RARE])

    def test_a_key_that_is_not_a_key_orders_by_the_tiebreak_rather_than_by_a_substitute(self) -> None:
        """Dropped rather than replaced: a page that fell back to convictions would present itself as
        sorted by the column the reader asked for."""
        self.assertEqual(order('not-a-column', True), ())
        self.assertEqual(self._names('not-a-column', descending=True), [self.OFTEN, self.RARE])
        self.assertEqual(self._names('not-a-column', descending=False), [self.OFTEN, self.RARE])


class TestTestConvictions(fixtures.DatabaseTest):
    """One test's own drilldown: every conviction it drew in the window, newest first, capped the
    same way `convicted_tests` caps its own rows."""

    TEST_NAME = 'fast/drilled.html'

    def test_one_row_per_conviction_newest_first(self) -> None:
        self.store_build(1, flaky={self.TEST_NAME: config.CLEAN_TREE},
                         started_at=fixtures.DEFAULT_BUILD_TIME)
        self.store_build(2, flaky={self.TEST_NAME: config.DIRTY_TREE},
                         started_at=fixtures.DEFAULT_BUILD_TIME + 600)
        drilldown = convictions.test_convictions(self.connection, *WINDOW, self.TEST_NAME)
        self.assertEqual([conviction.build_number for conviction in drilldown.convictions], [2, 1])
        self.assertEqual([conviction.rule for conviction in drilldown.convictions],
                         [config.DIRTY_TREE, config.CLEAN_TREE])

    def test_scoped_to_the_named_test(self) -> None:
        self.store_build(1, flaky={self.TEST_NAME: config.CLEAN_TREE})
        self.store_build(2, flaky={'fast/other.html': config.CLEAN_TREE})
        drilldown = convictions.test_convictions(self.connection, *WINDOW, self.TEST_NAME)
        self.assertEqual(len(drilldown.convictions), 1)
        self.assertEqual(drilldown.convictions[0].build_number, 1)

    def test_builds_outside_the_window_are_not_counted(self) -> None:
        self.store_build(1, flaky={self.TEST_NAME: config.CLEAN_TREE},
                         started_at=WINDOW[0] - 86400)
        drilldown = convictions.test_convictions(self.connection, *WINDOW, self.TEST_NAME)
        self.assertEqual((drilldown.convictions, drilldown.total), ((), 0))

    def test_a_test_with_no_convictions_reports_none(self) -> None:
        self.store_build(1, flaky={'fast/other.html': config.CLEAN_TREE})
        drilldown = convictions.test_convictions(self.connection, *WINDOW, self.TEST_NAME)
        self.assertEqual((drilldown.convictions, drilldown.total), ((), 0))

    def test_a_builder_filter_narrows_the_drilldown(self) -> None:
        self.store_build(1, flaky={self.TEST_NAME: config.CLEAN_TREE})
        self.store_build(2, flaky={self.TEST_NAME: config.CLEAN_TREE},
                         builder=fixtures.API_BUILDER, builder_id=9)
        drilldown = convictions.test_convictions(self.connection, *WINDOW, self.TEST_NAME,
                                                 builders=(fixtures.API_BUILDER,))
        self.assertEqual([conviction.builder for conviction in drilldown.convictions],
                         [fixtures.API_BUILDER])

    def test_the_limit_is_honoured(self) -> None:
        self.store_build(1, flaky={self.TEST_NAME: config.CLEAN_TREE},
                         started_at=fixtures.DEFAULT_BUILD_TIME)
        self.store_build(2, flaky={self.TEST_NAME: config.CLEAN_TREE},
                         started_at=fixtures.DEFAULT_BUILD_TIME + 600)
        drilldown = convictions.test_convictions(self.connection, *WINDOW, self.TEST_NAME, limit=1)
        self.assertEqual(len(drilldown.convictions), 1)
        self.assertEqual(drilldown.convictions[0].build_number, 2)


class TestTestConvictionsCap(fixtures.DatabaseTest):
    """The drilldown has the same hard cap as `convicted_tests`, and the same total-and-truncated
    pair so a reader can tell whether the cap cut its list."""

    TEST_NAME = 'fast/drilled.html'

    def _convict(self, count: int) -> None:
        for number in range(1, count + 1):
            self.store_build(number, flaky={self.TEST_NAME: config.CLEAN_TREE},
                             started_at=fixtures.DEFAULT_BUILD_TIME + number)

    def test_the_default_cap_is_two_hundred(self) -> None:
        self.assertEqual(inspect.signature(convictions.test_convictions).parameters['limit'].default,
                         200)

    def test_more_rows_than_the_cap_reports_the_whole_total_and_is_truncated(self) -> None:
        self._convict(5)
        drilldown = convictions.test_convictions(self.connection, *WINDOW, self.TEST_NAME, limit=3)
        self.assertEqual(len(drilldown.convictions), 3)
        self.assertEqual(drilldown.total, 5)
        self.assertTrue(drilldown.truncated)

    def test_fewer_rows_than_the_cap_reports_the_same_total_and_is_not_truncated(self) -> None:
        self._convict(2)
        drilldown = convictions.test_convictions(self.connection, *WINDOW, self.TEST_NAME, limit=10)
        self.assertEqual(drilldown.total, 2)
        self.assertFalse(drilldown.truncated)


class TestQueueActivity(fixtures.DatabaseTest):
    def test_a_queue_that_never_asked_is_still_listed(self) -> None:
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE})
        self.store_build(2, builder=fixtures.API_BUILDER, builder_id=9)
        activity = {queue.builder: queue for queue in convictions.queue_activity(self.connection, *WINDOW)}
        self.assertEqual(activity[fixtures.API_BUILDER].builds_queried, 0)
        self.assertEqual(activity[fixtures.API_BUILDER].convictions, 0)
        self.assertEqual(activity[fixtures.LAYOUT_BUILDER].builds_queried, 1)
        self.assertEqual(activity[fixtures.LAYOUT_BUILDER].convictions, 1)

    def test_query_failures_are_reported_per_queue(self) -> None:
        self.store_build(1, query_failed=['fast/a.html', 'fast/b.html'])
        activity = convictions.queue_activity(self.connection, *WINDOW)
        self.assertEqual(activity[0].query_failures, 2)

    def test_busiest_queue_first(self) -> None:
        self.store_build(1, flaky={})
        self.store_build(2, flaky={})
        self.store_build(3, flaky={}, builder=fixtures.API_BUILDER, builder_id=9)
        self.assertEqual([queue.builder for queue in convictions.queue_activity(self.connection, *WINDOW)],
                         [fixtures.LAYOUT_BUILDER, fixtures.API_BUILDER])
