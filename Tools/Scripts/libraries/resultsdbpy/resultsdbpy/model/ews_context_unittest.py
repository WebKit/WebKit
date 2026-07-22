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
        return self.model.ews_context.find_for_test(
            configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
            suite='layout-tests', test=test, recent=True, flaky=flaky,
        )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_stores_all_reported_results(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)

        for test, leaf in self.DEFAULT_TEST_RESULTS['results'].items():
            stored = list(self._find(test).values())[0][0]['result']
            self.assertEqual(stored, leaf)

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
        self.init_database(redis=redis, cassandra=cassandra, flaky_type='InterStep')

        results = self._find(self.REGRESSION_TEST, flaky=True)
        self.assertGreater(len(results), 0)

        entry = list(results.values())[0][0]
        self.assertEqual(entry['result']['actual'], 'TEXT')
        self.assertEqual(entry['flaky_type'], 'InterStep')
        self.assertEqual(len(self._find(self.REGRESSION_TEST, flaky=False)), 0)

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
            results = self.model.ews_context.find_for_test(
                configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
                suite='layout-tests', test=self.REGRESSION_TEST, flaky=False, recent=False,
                begin_query_time=cutoff,
            )
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
