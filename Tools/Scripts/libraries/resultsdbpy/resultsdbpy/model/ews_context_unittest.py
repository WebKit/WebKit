# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import time

from fakeredis import FakeStrictRedis
from redis import StrictRedis
from resultsdbpy.controller.configuration import Configuration
from resultsdbpy.model.cassandra_context import CassandraContext
from resultsdbpy.model.mock_model_factory import MockModelFactory
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase


class EWSContextTest(WaitForDockerTestCase):
    KEYSPACE = 'ews_context_test_keyspace'

    DEFAULT_TEST_RESULTS = dict(
        results={
            'fast/encoding/css-cached-bom.html': {'report': 'REGRESSION', 'expected': 'PASS', 'actual': 'TEXT'},
            'fast/encoding/css-charset.html': {'report': 'FLAKY', 'expected': 'PASS', 'actual': 'TIMEOUT PASS', 'has_stderr': True},
            'fast/encoding/css-link-charset.html': {'report': 'MISSING', 'expected': 'PASS', 'actual': 'MISSING', 'is_missing_text': True},
        },
    )
    REGRESSION_TEST = 'fast/encoding/css-cached-bom.html'

    def init_database(self, redis=StrictRedis, cassandra=CassandraContext, test_results=None, timestamps=None, flaky_type=None, details=None):
        if timestamps is None:
            timestamps = [int(time.time())]

        with MockModelFactory.safari(), MockModelFactory.webkit():
            cassandra.drop_keyspace(keyspace=self.KEYSPACE)
            self.model = MockModelFactory.create(redis=redis(), cassandra=cassandra(keyspace=self.KEYSPACE, create_keyspace=True))

            for timestamp in timestamps:
                MockModelFactory.add_mock_ews_results(
                    test_results=test_results or self.DEFAULT_TEST_RESULTS, model=self.model,
                    flaky_type=flaky_type, details=details, timestamp=timestamp
                )

    def _find(self, test, flaky=False):
        return self.model.ews_context.find_for_tests(
            configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
            suite='layout-tests', tests=[test], recent=True, flaky=flaky,
        ).get(test, {})

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_ttl_is_an_integer(self, redis=StrictRedis, cassandra=CassandraContext):
        """cqlengine formats the TTL into the CQL verbatim, so a float reaches Cassandra as an InvalidRequest."""
        self.init_database(redis=redis, cassandra=cassandra)

        ttls = []
        context = self.model.ews_context
        originals = [
            (context.cassandra, 'insert_row'),
            (context.configuration_context, 'insert_row_with_configuration'),
        ]

        def record(original):
            def wrapper(*args, ttl=None, **kwargs):
                ttls.append(ttl)
                return original(*args, ttl=ttl, **kwargs)
            return wrapper

        try:
            for owner, name in originals:
                setattr(owner, name, record(getattr(owner, name)))
            MockModelFactory.add_mock_ews_results(test_results=self.DEFAULT_TEST_RESULTS, model=self.model)
        finally:
            for owner, name in originals:
                delattr(owner, name)

        self.assertTrue(ttls)
        for ttl in ttls:
            self.assertIsInstance(ttl, (int, type(None)))
            self.assertNotIsInstance(ttl, bool)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_stores_all_reported_results(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)

        for test, leaf in self.DEFAULT_TEST_RESULTS['results'].items():
            stored = list(self._find(test).values())[0][0]['result']
            self.assertEqual(stored, leaf)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_several_tests_are_answered_in_one_call(self, redis=StrictRedis, cassandra=CassandraContext):
        """Keyed by test name, so a caller asking about a batch can tell the answers apart."""
        self.init_database(redis=redis, cassandra=cassandra)
        reported = sorted(self.DEFAULT_TEST_RESULTS['results'])

        found = self.model.ews_context.find_for_tests(
            configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
            suite='layout-tests', tests=reported + ['fast/never/reported.html'],
            recent=True, flaky=False,
        )

        # A test with nothing recorded is absent rather than present and empty.
        self.assertEqual(sorted(found), reported)
        for test in reported:
            rows = [row for rows in found[test].values() for row in rows]
            self.assertTrue(rows, test)
            for row in rows:
                self.assertEqual(row['result'], self.DEFAULT_TEST_RESULTS['results'][test])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_the_limit_is_spent_per_test_rather_than_shared(self, redis=StrictRedis, cassandra=CassandraContext):
        """Shared across the batch, the limit would be spent on whichever test was queried first and
        every later test would come back empty. It bounds each test and configuration separately."""
        base = int(time.time())
        self.init_database(redis=redis, cassandra=cassandra, timestamps=[base - 3600, base])
        reported = sorted(self.DEFAULT_TEST_RESULTS['results'])
        self.assertGreater(len(reported), 1)

        def rows_per_configuration(limit):
            found = self.model.ews_context.find_for_tests(
                configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
                suite='layout-tests', tests=reported, recent=True, flaky=False, limit=limit,
            )
            self.assertEqual(sorted(found), reported)
            return {test: sorted(len(rows) for rows in by_configuration.values()) for test, by_configuration in found.items()}

        # Every test has more rows than the limit will allow, so a limit that is ignored would show.
        unlimited = rows_per_configuration(100)
        for test, counts in unlimited.items():
            self.assertTrue(counts, test)
            self.assertTrue(all(count > 1 for count in counts), f'{test}: {counts}')

        limited = rows_per_configuration(1)
        for test, counts in limited.items():
            self.assertEqual(counts, [1] * len(unlimited[test]), test)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_the_configurations_are_expanded_once_for_the_batch(self, redis=StrictRedis, cassandra=CassandraContext):
        """EWS never queries with a complete configuration, so every read has to expand a partial one
        against redis. That expansion does not depend on the test, so a batch must only pay it once."""
        self.init_database(redis=redis, cassandra=cassandra)
        reported = sorted(self.DEFAULT_TEST_RESULTS['results'])
        self.assertGreater(len(reported), 1)

        expansions = []
        configuration_context = self.model.ews_context.configuration_context
        original = configuration_context.search_for_recent_configuration

        def record(configuration, **kwargs):
            expansions.append(configuration)
            return original(configuration, **kwargs)

        try:
            configuration_context.search_for_recent_configuration = record
            found = self.model.ews_context.find_for_tests(
                configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
                suite='layout-tests', tests=reported, recent=True, flaky=False,
            )
        finally:
            del configuration_context.search_for_recent_configuration

        self.assertEqual(sorted(found), reported)
        self.assertEqual(len(expansions), 1)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_metadata_stored(self, redis=StrictRedis, cassandra=CassandraContext):
        details = dict(remote='github.com/WebKit/WebKit', pr_number=12345, commit_hash='abc123def456', build_number=678)
        self.init_database(redis=redis, cassandra=cassandra, details=details)

        entry = list(self._find(self.REGRESSION_TEST).values())[0][0]
        stored = entry['details']
        self.assertEqual(stored.get('remote'), 'github.com/WebKit/WebKit')
        self.assertEqual(stored.get('pr_number'), 12345)
        self.assertEqual(stored.get('commit_hash'), 'abc123def456')
        self.assertEqual(stored.get('build_number'), 678)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_flaky_results_stored(self, redis=StrictRedis, cassandra=CassandraContext):
        """Flaky results land in a separate table, tagged with their flaky_type. These should not be stored in the failure table."""
        self.init_database(redis=redis, cassandra=cassandra, flaky_type='BetweenStepsDirtyTree')

        results = self._find(self.REGRESSION_TEST, flaky=True)
        self.assertGreater(len(results), 0)

        entry = list(results.values())[0][0]
        self.assertEqual(entry['result']['actual'], 'TEXT')
        self.assertEqual(entry['flaky_type'], 'BetweenStepsDirtyTree')
        self.assertEqual(len(self._find(self.REGRESSION_TEST, flaky=False)), 0)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_both_flaky_types_from_one_run_are_kept(self, redis=StrictRedis, cassandra=CassandraContext):
        """A re-run reports a test flaky within its own step and flaky across the two steps. Those
        share a start_time and a commit, so flaky_type has to be part of the key to keep both."""
        timestamp = int(time.time())
        self.init_database(redis=redis, cassandra=cassandra, flaky_type='WithinStepDirtyTree', timestamps=[timestamp])

        with MockModelFactory.safari(), MockModelFactory.webkit():
            MockModelFactory.add_mock_ews_results(
                test_results=self.DEFAULT_TEST_RESULTS, model=self.model,
                flaky_type='BetweenStepsDirtyTree', timestamp=timestamp,
            )

        rows = list(self._find(self.REGRESSION_TEST, flaky=True).values())[0]
        types_by_run = {}
        for row in rows:
            types_by_run.setdefault((row['uuid'], row['start_time']), set()).add(row['flaky_type'])

        self.assertTrue(types_by_run)
        for run, flaky_types in types_by_run.items():
            self.assertEqual(flaky_types, {'WithinStepDirtyTree', 'BetweenStepsDirtyTree'}, run)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_retries_preserved_for_same_commit(self, redis=StrictRedis, cassandra=CassandraContext):
        """EWS may run the same commit more than once. Each run should be preserved as a distinct row in the database."""
        base = int(time.time())
        first, second = base - 3600, base
        self.init_database(redis=redis, cassandra=cassandra, timestamps=[first, second])

        results = self._find(self.REGRESSION_TEST)
        self.assertGreater(len(results), 0)
        start_times_by_uuid = {}

        for row in list(results.values())[0]:
            start_times_by_uuid.setdefault(row['uuid'], set()).add(row['start_time'])
        self.assertTrue(any({first, second}.issubset(times) for times in start_times_by_uuid.values()))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_time_window_query(self, redis=StrictRedis, cassandra=CassandraContext):
        HOUR = 60 * 60
        now = int(time.time())
        recent, day_old, old = now - HOUR, now - 30 * HOUR, now - 72 * HOUR
        self.init_database(redis=redis, cassandra=cassandra, timestamps=[recent, day_old, old])

        def start_times_within(cutoff):
            results = self.model.ews_context.find_for_tests(
                configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
                suite='layout-tests', tests=[self.REGRESSION_TEST], flaky=False, recent=False,
                begin_query_time=cutoff,
            ).get(self.REGRESSION_TEST, {})
            return {row['start_time'] for rows in results.values() for row in rows}

        self.assertEqual(start_times_within(now - 24 * HOUR), {recent})
        self.assertEqual(start_times_within(now - 48 * HOUR), {recent, day_old})
        self.assertEqual(start_times_within(now - 96 * HOUR), {recent, day_old, old})

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_no_results(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            cassandra.drop_keyspace(keyspace=self.KEYSPACE)
            self.model = MockModelFactory.create(redis=redis(), cassandra=cassandra(keyspace=self.KEYSPACE, create_keyspace=True))
        self.assertEqual(len(self._find(self.REGRESSION_TEST)), 0)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_record_results_reports_the_tests_it_stored(self, redis=StrictRedis, cassandra=CassandraContext):
        """Callers cannot see the write, so registering has to tell them which tests it recorded."""
        with MockModelFactory.safari(), MockModelFactory.webkit() as webkit:
            cassandra.drop_keyspace(keyspace=self.KEYSPACE)
            self.model = MockModelFactory.create(redis=redis(), cassandra=cassandra(keyspace=self.KEYSPACE, create_keyspace=True))

            stored = self.model.ews_context.record_results(
                Configuration(platform='Mac', version='13.0.0', is_simulator=False, architecture='arm64', style='Release', flavor='wk1'),
                [webkit.commits['main'][-1]], 'layout-tests', self.DEFAULT_TEST_RESULTS,
            )

        self.assertEqual(stored, sorted(self.DEFAULT_TEST_RESULTS['results']))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_record_results_raises_when_the_write_is_rejected(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit() as webkit:
            cassandra.drop_keyspace(keyspace=self.KEYSPACE)
            self.model = MockModelFactory.create(redis=redis(), cassandra=cassandra(keyspace=self.KEYSPACE, create_keyspace=True))

            # A partial configuration cannot be written, and must not be reported as stored.
            with self.assertRaises(TypeError):
                self.model.ews_context.record_results(
                    Configuration(platform='Mac', style='Release'),
                    [webkit.commits['main'][-1]], 'layout-tests', self.DEFAULT_TEST_RESULTS,
                )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_names_enumerates_reported_tests(self, redis=StrictRedis, cassandra=CassandraContext):
        """Results can only be read back by exact test name, so registering must record the names."""
        self.init_database(redis=redis, cassandra=cassandra)

        self.assertEqual(
            sorted(self.model.ews_context.names(suite='layout-tests', flaky=False)),
            sorted(self.DEFAULT_TEST_RESULTS['results']),
        )
        self.assertEqual(sorted(self.model.ews_context.names(suite='layout-tests', flaky=True)), [])
        self.assertEqual(sorted(self.model.ews_context.names(suite='javascriptcore-tests', flaky=False)), [])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_names_separates_flaky_from_failed(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra, flaky_type='WithinStepDirtyTree')

        expected = sorted(self.DEFAULT_TEST_RESULTS['results'])
        self.assertEqual(sorted(self.model.ews_context.names(suite='layout-tests', flaky=True)), expected)
        self.assertEqual(sorted(self.model.ews_context.names(suite='layout-tests', flaky=False)), [])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_names_filters_by_prefix(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra, test_results=dict(results={
            'fast/encoding/css-cached-bom.html': {'expected': 'PASS', 'actual': 'TEXT'},
            'fast/forms/submit.html': {'expected': 'PASS', 'actual': 'TEXT'},
            'http/tests/xhr/basic.html': {'expected': 'PASS', 'actual': 'TIMEOUT'},
        }))

        self.assertEqual(
            sorted(self.model.ews_context.names(suite='layout-tests', test='fast/', flaky=False)),
            ['fast/encoding/css-cached-bom.html', 'fast/forms/submit.html'],
        )
        self.assertEqual(
            sorted(self.model.ews_context.names(suite='layout-tests', test='http/', flaky=False)),
            ['http/tests/xhr/basic.html'],
        )
        self.assertEqual(sorted(self.model.ews_context.names(suite='layout-tests', test='nonexistent/', flaky=False)), [])
