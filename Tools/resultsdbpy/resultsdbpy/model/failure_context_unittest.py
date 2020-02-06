# Copyright (C) 2019 Apple Inc. All rights reserved.
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
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.mock_model_factory import MockModelFactory
from resultsdbpy.model.mock_repository import MockSVNRepository
from resultsdbpy.model.test_context import Expectations
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase


class FailureContextTest(WaitForDockerTestCase):
    KEYSPACE = 'failure_context_test_keyspace'

    def init_database(self, redis=StrictRedis, cassandra=CassandraContext):
        cassandra.drop_keyspace(keyspace=self.KEYSPACE)
        self.model = MockModelFactory.create(redis=redis(), cassandra=cassandra(keyspace=self.KEYSPACE, create_keyspace=True))
        MockModelFactory.add_mock_results(self.model, test_results=dict(
            details=dict(link='dummy-link'),
            run_stats=dict(tests_skipped=0),
            results={
                'fast': {
                    'encoding': {
                        'css-cached-bom.html': dict(expected='PASS', actual='FAIL', time=1.2),
                        'css-charset-default.xhtml': dict(expected='FAIL', actual='FAIL', time=1.2),
                        'css-charset.html': dict(expected='FAIL', actual='PASS', time=1.2),
                        'css-link-charset.html': dict(expected='PASS', actual='PASS', time=1.2),
                    }
                }
            },
        ))
        MockModelFactory.process_results(self.model)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_failures_collapsed(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        results = self.model.failure_context.failures_by_commit(
            configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
            suite='layout-tests', recent=True, unexpected=False,
        )

        self.assertEqual(len(results), 2)
        self.assertEqual(results, set(['fast/encoding/css-cached-bom.html', 'fast/encoding/css-charset-default.xhtml']))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_unexpected_failures_collapsed(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        results = self.model.failure_context.failures_by_commit(
            configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
            suite='layout-tests', recent=True,
        )

        self.assertEqual(len(results), 1)
        self.assertEqual(results, set(['fast/encoding/css-cached-bom.html']))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_failures(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        results = self.model.failure_context.failures_by_commit(
            configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
            suite='layout-tests', recent=True, collapsed=False, unexpected=False,
        )

        self.assertEqual(len(results), 1)
        self.assertEqual(len(list(results.values())[0]), 5)
        self.assertEqual(sorted(list(results.values())[0], key=lambda value: value['uuid'])[0], {
            'fast/encoding/css-cached-bom.html': 'FAIL',
            'fast/encoding/css-charset-default.xhtml': 'FAIL',
            'start_time': list(results.values())[0][0]['start_time'],
            'uuid': 153802947900
        })

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_unexpected_failures(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        results = self.model.failure_context.failures_by_commit(
            configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
            suite='layout-tests', recent=True, collapsed=False
        )

        self.assertEqual(len(results), 1)
        self.assertEqual(len(list(results.values())[0]), 5)
        self.assertEqual(sorted(list(results.values())[0], key=lambda value: value['uuid'])[0], {
            'fast/encoding/css-cached-bom.html': 'FAIL',
            'start_time': list(results.values())[0][0]['start_time'],
            'uuid': 153802947900
        })

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_no_failures(self, redis=StrictRedis, cassandra=CassandraContext):
        cassandra.drop_keyspace(keyspace=self.KEYSPACE)
        self.model = MockModelFactory.create(redis=redis(), cassandra=cassandra(keyspace=self.KEYSPACE, create_keyspace=True))
        MockModelFactory.add_mock_results(self.model)
        MockModelFactory.process_results(self.model)
        results = self.model.failure_context.failures_by_commit(
            configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
            suite='layout-tests', recent=True, collapsed=False, unexpected=False,
        )
        self.assertEqual(len(results), 0)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_no_test_runs(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        results = self.model.failure_context.failures_by_commit(
            configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
            suite='layout-tests', recent=True, end=0,
        )
        self.assertEqual(results, None)
