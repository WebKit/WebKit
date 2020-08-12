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

from fakeredis import FakeStrictRedis
from redis import StrictRedis
from resultsdbpy.controller.configuration import Configuration
from resultsdbpy.model.cassandra_context import CassandraContext
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.mock_model_factory import MockModelFactory
from resultsdbpy.model.upload_context import UploadContext
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase


class UploadContextTest(WaitForDockerTestCase):
    KEYSPACE = 'upload_context_test_keyspace'

    def init_database(self, redis=StrictRedis, cassandra=CassandraContext, async_processing=False):
        cassandra.drop_keyspace(keyspace=self.KEYSPACE)
        self.model = MockModelFactory.create(
            redis=redis(), cassandra=cassandra(keyspace=self.KEYSPACE, create_keyspace=True),
            async_processing=async_processing,
        )

    def test_zipping(self):
        zipped_bytes = UploadContext.to_zip('somestring' * 100)
        self.assertNotEqual(zipped_bytes, 'somestring' * 100)
        self.assertEqual(UploadContext.from_zip(zipped_bytes), 'somestring' * 100)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_suite_list(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)

        MockModelFactory.add_mock_results(self.model)
        for suites in self.model.upload_context.find_suites(configurations=[Configuration()], recent=True).values():
            self.assertEqual(suites, ['layout-tests'])

        MockModelFactory.add_mock_results(self.model, suite='api_tests')
        for suites in self.model.upload_context.find_suites(configurations=[Configuration()], recent=True).values():
            self.assertEqual(suites, ['api_tests', 'layout-tests'])

        MockModelFactory.add_mock_results(self.model, configuration=Configuration(is_simulator=True), suite='webkitpy')
        for suites in self.model.upload_context.find_suites(configurations=[Configuration(is_simulator=True)], recent=True).values():
            self.assertEqual(suites, ['api_tests', 'layout-tests', 'webkitpy'])
        for suites in self.model.upload_context.find_suites(configurations=[Configuration(is_simulator=False)], recent=True).values():
            self.assertEqual(suites, ['api_tests', 'layout-tests'])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_result_retrieval(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        MockModelFactory.add_mock_results(self.model)

        results = self.model.upload_context.find_test_results(configurations=[Configuration(platform='Mac')], suite='layout-tests', recent=True)
        self.assertEqual(6, len(results))
        for config, values in results.items():
            self.assertEqual(config, Configuration(platform='Mac'))
            self.assertEqual(5, len(values))
            for value in values:
                self.assertEqual(value['test_results'], MockModelFactory.layout_test_results())

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_result_retrieval_limit(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        MockModelFactory.add_mock_results(self.model)

        results = self.model.upload_context.find_test_results(configurations=[Configuration(platform='Mac')], suite='layout-tests', limit=2, recent=True)
        self.assertEqual(sum([len(value) for value in results.values()]), 12)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_result_retrieval_branch(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        MockModelFactory.add_mock_results(self.model)

        results = self.model.upload_context.find_test_results(configurations=[Configuration(platform='iOS', is_simulator=True)], suite='layout-tests', branch='safari-606-branch', recent=True)
        self.assertEqual(3, len(results))
        for config, values in results.items():
            self.assertEqual(config, Configuration(platform='iOS', is_simulator=True))
            self.assertEqual(2, len(values))
            for value in values:
                self.assertEqual(value['test_results'], MockModelFactory.layout_test_results())

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_result_retrieval_by_sdk(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        MockModelFactory.add_mock_results(self.model)

        self.assertEqual(0, len(self.model.upload_context.find_test_results(configurations=[Configuration(platform='iOS', sdk='15A432')], suite='layout-tests', recent=True)))
        results = self.model.upload_context.find_test_results(configurations=[Configuration(platform='iOS', sdk='15A432')], suite='layout-tests', recent=False)
        self.assertEqual(6, len(results))
        for config, values in results.items():
            self.assertEqual(config.version, 11000000)
            for value in values:
                self.assertEqual(value['sdk'], '15A432')
                self.assertEqual(value['test_results'], MockModelFactory.layout_test_results())

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_sdk_differentiation(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        MockModelFactory.add_mock_results(self.model)

        configuration_to_search = Configuration(platform='iOS', version='12.0.0', is_simulator=True, style='Asan')
        results = self.model.upload_context.find_test_results(configurations=[configuration_to_search], suite='layout-tests', recent=False)
        self.assertEqual(1, len(results))

        MockModelFactory.add_mock_results(self.model, configuration=Configuration(
            platform='iOS', version='12.0.0', sdk='16A405', is_simulator=True, architecture='x86_64', style='Asan',
        ))

        results = self.model.upload_context.find_test_results(configurations=[configuration_to_search], suite='layout-tests', recent=False)
        self.assertEqual(2, len(results))
        results = self.model.upload_context.find_test_results(configurations=[Configuration(platform='iOS', sdk='16A405')], suite='layout-tests', recent=False)
        self.assertEqual(1, len(results))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_callback(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        MockModelFactory.add_mock_results(self.model)

        configuration_to_search = Configuration(platform='iOS', version='12.0.0', is_simulator=True, style='Asan')
        configuration, uploads = next(iter(self.model.upload_context.find_test_results(configurations=[configuration_to_search], suite='layout-tests', recent=False).items()))
        self.model.upload_context.process_test_results(
            configuration=configuration,
            commits=uploads[0]['commits'],
            suite='layout-tests',
            test_results=uploads[0]['test_results'],
            timestamp=uploads[0]['timestamp'],
        )

        # Using suite results as a proxy to tell if callbacks were triggered
        self.assertEqual(1, len(self.model.suite_context.find_by_commit(configurations=[Configuration()], suite='layout-tests')))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_async_callback(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra, async_processing=True)
        MockModelFactory.add_mock_results(self.model)

        configuration_to_search = Configuration(platform='iOS', version='12.0.0', is_simulator=True, style='Asan')
        configuration, uploads = next(iter(self.model.upload_context.find_test_results(configurations=[configuration_to_search], suite='layout-tests', recent=False).items()))
        self.model.upload_context.process_test_results(
            configuration=configuration,
            commits=uploads[0]['commits'],
            suite='layout-tests',
            test_results=uploads[0]['test_results'],
            timestamp=uploads[0]['timestamp'],
        )

        # Using suite results as a proxy to tell if callbacks were triggered
        self.assertEqual(0, len(self.model.suite_context.find_by_commit(configurations=[Configuration()], suite='layout-tests')))
        self.assertTrue(self.model.upload_context.do_processing_work())
        self.assertEqual(1, len(self.model.suite_context.find_by_commit(configurations=[Configuration()], suite='layout-tests')))
