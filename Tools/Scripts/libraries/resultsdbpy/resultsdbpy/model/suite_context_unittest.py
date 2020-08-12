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


class SuiteContextTest(WaitForDockerTestCase):
    KEYSPACE = 'suite_context_test_keyspace'

    def init_database(self, redis=StrictRedis, cassandra=CassandraContext, test_results=None):
        cassandra.drop_keyspace(keyspace=self.KEYSPACE)
        self.model = MockModelFactory.create(redis=redis(), cassandra=cassandra(keyspace=self.KEYSPACE, create_keyspace=True))
        MockModelFactory.add_mock_results(self.model, test_results=test_results)
        MockModelFactory.process_results(self.model)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_find_all(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)

        results = self.model.suite_context.find_by_commit(
            configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
            suite='layout-tests', recent=True,
        )
        self.assertEqual(len(results), 1)
        self.assertEqual(len(next(iter(results.values()))), 5)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_find_by_commit(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)

        results = self.model.suite_context.find_by_commit(
            configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
            suite='layout-tests', recent=False,
            begin=MockSVNRepository.webkit().commit_for_id(236542), end=MockSVNRepository.webkit().commit_for_id(236543),
        )
        self.assertEqual(len(results), 2)
        for results_for_config in results.values():
            self.assertEqual(len(results_for_config), 2)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_find_by_time(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)

        results = self.model.suite_context.find_by_start_time(
            configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
            suite='layout-tests', recent=False,
            begin_query_time=(time.time() - 60 * 60),
        )

        self.assertEqual(len(results), 1)
        self.assertEqual(len(next(iter(results.values()))), 5)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_all_successful(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)

        results = self.model.suite_context.find_by_start_time(
            configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
            suite='layout-tests',
            begin=MockSVNRepository.webkit().commit_for_id(236542), end=MockSVNRepository.webkit().commit_for_id(236542),
        )

        self.assertEqual(
            next(iter(results.values()))[0]['stats'],
            dict(
                tests_run=4,
                tests_skipped=0,
                tests_failed=0,
                tests_timedout=0,
                tests_crashed=0,
                tests_unexpected_failed=0,
                tests_unexpected_timedout=0,
                tests_unexpected_crashed=0,
            ),
        )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_failure_expectations(self, redis=StrictRedis, cassandra=CassandraContext):
        test_results = MockModelFactory.layout_test_results()
        test_results['results']['fast']['encoding']['css-cached-bom.html'] = dict(
            actual=Expectations.FAIL,
            time=1.2,
        )
        test_results['results']['fast']['encoding']['css-charset-default.xhtml'] = dict(
            actual=Expectations.TIMEOUT,
            expected=Expectations.TIMEOUT,
            time=1.2,
        )
        self.init_database(redis=redis, cassandra=cassandra, test_results=test_results)

        results = self.model.suite_context.find_by_start_time(
            configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
            suite='layout-tests',
            begin=MockSVNRepository.webkit().commit_for_id(236542),
            end=MockSVNRepository.webkit().commit_for_id(236542),
        )

        self.assertEqual(
            next(iter(results.values()))[0]['stats'],
            dict(
                tests_run=4,
                tests_skipped=0,
                tests_failed=2,
                tests_timedout=1,
                tests_crashed=0,
                tests_unexpected_failed=1,
                tests_unexpected_timedout=0,
                tests_unexpected_crashed=0,
            ),
        )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_image_diff_as_failure_expectations(self, redis=StrictRedis, cassandra=CassandraContext):
        test_results = MockModelFactory.layout_test_results()
        test_results['results']['fast']['encoding']['css-cached-bom.html'] = dict(
            actual=Expectations.TIMEOUT,
            expected=Expectations.FAIL,
            time=1.2,
        )
        test_results['results']['fast']['encoding']['css-charset-default.xhtml'] = dict(
            actual=Expectations.IMAGE,
            expected=Expectations.FAIL,
            time=1.2,
        )
        self.init_database(redis=redis, cassandra=cassandra, test_results=test_results)

        results = self.model.suite_context.find_by_start_time(
            configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
            suite='layout-tests',
            begin=MockSVNRepository.webkit().commit_for_id(236542),
            end=MockSVNRepository.webkit().commit_for_id(236542),
        )

        self.assertEqual(
            next(iter(results.values()))[0]['stats'],
            dict(
                tests_run=4,
                tests_skipped=0,
                tests_failed=2,
                tests_timedout=1,
                tests_crashed=0,
                tests_unexpected_failed=1,
                tests_unexpected_timedout=1,
                tests_unexpected_crashed=0,
            ),
        )
