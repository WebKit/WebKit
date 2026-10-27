# Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
from resultsdbpy.model.test_context import Expectations
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase


class TestContextTest(WaitForDockerTestCase):
    KEYSPACE = 'test_context_test_keyspace'
    DETAILS_WITH_LINKS = dict(link='https://ci.example.com/build/1', build_link='https://ci.example.com/build/1', post_build_link='https://ci.example.com/post-build/1')
    BUILDBOT_DETAILS = {'buildbot-master': 'build.webkit.org', 'builder-name': 'Catalina-Release-WK1-Tests', 'build-number': '1', 'buildbot-worker': 'bot2'}

    def init_database(self, redis=StrictRedis, cassandra=CassandraContext, test_results=None):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            cassandra.drop_keyspace(keyspace=self.KEYSPACE)
            self.model = MockModelFactory.create(redis=redis(), cassandra=cassandra(keyspace=self.KEYSPACE, create_keyspace=True))
            MockModelFactory.add_mock_results(self.model, test_results=test_results)
            MockModelFactory.process_results(self.model)

    def init_database_with_details(self, details, redis=StrictRedis, cassandra=CassandraContext):
        test_results = MockModelFactory.layout_test_results()
        test_results['details'] = details
        self.init_database(redis=redis, cassandra=cassandra, test_results=test_results)

    def results_for_test(self):
        results = self.model.test_context.find_by_commit(
            configurations=[Configuration(platform='mac', style='Release', flavor='wk1')],
            suite='layout-tests', test='fast/encoding/css-link-charset.html', recent=True,
        )
        return next(iter(results.values()))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_list_all(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        results = self.model.test_context.names(suite='layout-tests')
        self.assertEqual(results, [
            'fast/encoding/css-cached-bom.html',
            'fast/encoding/css-charset-default.xhtml',
            'fast/encoding/css-charset.html',
            'fast/encoding/css-link-charset.html',
        ])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_list_partial(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        results = self.model.test_context.names(suite='layout-tests', test='fast/encoding/css-charset')
        self.assertEqual(results, [
            'fast/encoding/css-charset-default.xhtml',
            'fast/encoding/css-charset.html',
        ])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_find_all(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        results = self.model.test_context.find_by_commit(
            configurations=[Configuration(platform='mac', style='Release', flavor='wk1')],
            suite='layout-tests', test='fast/encoding/css-link-charset.html', recent=True,
        )

        self.assertEqual(len(results), 1)
        self.assertEqual(len(next(iter(results.values()))), 5)
        for result in next(iter(results.values())):
            self.assertEqual(result['actual'], Expectations.PASS)
            self.assertEqual(result['expected'], Expectations.PASS)
            self.assertEqual(result['time'], 1.2)
            self.assertEqual(result['modifiers'], '')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_number_of_links(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database_with_details(self.DETAILS_WITH_LINKS, redis=redis, cassandra=cassandra)
        results = self.results_for_test()
        self.assertEqual(len(results), 5)
        for result in results:
            self.assertEqual(sorted(result.keys()), ['actual', 'expected', 'modifiers', 'number_of_links', 'start_time', 'time', 'uuid'])
            self.assertEqual(result['number_of_links'], 2)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_no_number_of_links_for_link_alone(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database_with_details(dict(link='https://ci.example.com/build/1'), redis=redis, cassandra=cassandra)
        for result in self.results_for_test():
            self.assertNotIn('number_of_links', result)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_no_number_of_links_for_buildbot(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database_with_details(self.BUILDBOT_DETAILS, redis=redis, cassandra=cassandra)
        for result in self.results_for_test():
            self.assertNotIn('number_of_links', result)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_find_by_commit(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        results = self.model.test_context.find_by_commit(
            configurations=[Configuration(platform='mac', style='Release', flavor='wk1')],
            suite='layout-tests', test='fast/encoding/css-cached-bom.html', recent=False,
            begin=1601661000,
            end=1601663000,
        )
        self.assertEqual(len(results), 2)
        for results_for_config in results.values():
            self.assertEqual(len(results_for_config), 2)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_find_by_time(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        results = self.model.test_context.find_by_start_time(
            configurations=[Configuration(platform='mac', style='Release', flavor='wk1')],
            suite='layout-tests', test='fast/encoding/css-charset.html', recent=False,
            begin_query_time=(time.time() - 60 * 60),
        )

        self.assertEqual(len(results), 1)
        self.assertEqual(len(next(iter(results.values()))), 5)
