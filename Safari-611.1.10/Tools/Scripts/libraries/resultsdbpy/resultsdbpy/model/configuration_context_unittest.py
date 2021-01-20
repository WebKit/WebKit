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

from cassandra.cqlengine import columns
from resultsdbpy.controller.configuration import Configuration
from fakeredis import FakeStrictRedis
from redis import StrictRedis
from resultsdbpy.model.cassandra_context import CassandraContext
from resultsdbpy.model.commit_context import CommitContext
from resultsdbpy.model.configuration_context import ConfigurationContext, ClusteredByConfiguration
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase


class ConfigurationContextTest(WaitForDockerTestCase):
    KEYSPACE = 'configuration_context_test_keyspace'
    CONFIGURATIONS = [
        Configuration(platform='Mac', version='10.13.0', sdk='17A405', is_simulator=False, architecture='x86_64', style='Debug', flavor='wk1'),
        Configuration(platform='Mac', version='10.13.0', sdk='17A405', is_simulator=False, architecture='x86_64', style='Debug', flavor='wk2'),
        Configuration(platform='Mac', version='10.13.0', sdk='17A405', is_simulator=False, architecture='x86_64', style='Release', flavor='wk1'),
        Configuration(platform='Mac', version='10.13.0', sdk='17A405', is_simulator=False, architecture='x86_64', style='Release', flavor='wk2'),
        Configuration(platform='Mac', version='10.13.0', sdk='17A405', is_simulator=False, architecture='x86_64', style='Asan', flavor='wk1'),
        Configuration(platform='Mac', version='10.13.0', sdk='17A405', is_simulator=False, architecture='x86_64', style='Asan', flavor='wk2'),
        Configuration(platform='Mac', version='10.14.0', sdk='18A391', is_simulator=False, architecture='x86_64', style='Debug', flavor='wk1'),
        Configuration(platform='Mac', version='10.14.0', sdk='18A391', is_simulator=False, architecture='x86_64', style='Debug', flavor='wk2'),
        Configuration(platform='Mac', version='10.14.0', sdk='18A391', is_simulator=False, architecture='x86_64', style='Release', flavor='wk1'),
        Configuration(platform='Mac', version='10.14.0', sdk='18A391', is_simulator=False, architecture='x86_64', style='Release', flavor='wk2'),
        Configuration(platform='Mac', version='10.14.0', sdk='18A391', is_simulator=False, architecture='x86_64', style='Asan', flavor='wk1'),
        Configuration(platform='Mac', version='10.14.0', sdk='18A391', is_simulator=False, architecture='x86_64', style='Asan', flavor='wk2'),
        Configuration(platform='iOS', version='11.0.0', sdk='15A432', is_simulator=True, architecture='x86_64', style='Debug'),
        Configuration(platform='iOS', version='11.0.0', sdk='15A432', is_simulator=True, architecture='x86_64', style='Release'),
        Configuration(platform='iOS', version='11.0.0', sdk='15A432', is_simulator=True, architecture='x86_64', style='Asan'),
        Configuration(platform='iOS', version='11.0.0', sdk='15A432', is_simulator=False, architecture='arm64', model='iPhone 8', style='Debug'),
        Configuration(platform='iOS', version='11.0.0', sdk='15A432', is_simulator=False, architecture='arm64', model='iPhone 8', style='Release'),
        Configuration(platform='iOS', version='11.0.0', sdk='15A432', is_simulator=False, architecture='arm64', model='iPhone 8', style='Asan'),
        Configuration(platform='iOS', version='12.0.0', sdk='16A404', is_simulator=True, architecture='x86_64', style='Debug'),
        Configuration(platform='iOS', version='12.0.0', sdk='16A404', is_simulator=True, architecture='x86_64', style='Release'),
        Configuration(platform='iOS', version='12.0.0', sdk='16A404', is_simulator=True, architecture='x86_64', style='Asan'),
        Configuration(platform='iOS', version='12.0.0', sdk='16A404', is_simulator=False, architecture='arm64', model='iPhone Xs', style='Debug'),
        Configuration(platform='iOS', version='12.0.0', sdk='16A404', is_simulator=False, architecture='arm64', model='iPhone 8', style='Release'),
        Configuration(platform='iOS', version='12.0.0', sdk='16A404', is_simulator=False, architecture='arm64', model='iPhone Xs', style='Asan'),
    ]

    def init_database(self, redis=StrictRedis, cassandra=CassandraContext):
        cassandra.drop_keyspace(keyspace=self.KEYSPACE)
        self.database = ConfigurationContext(
            redis=redis(),
            cassandra=cassandra(keyspace=self.KEYSPACE, create_keyspace=True),
        )

    def register_configurations(self):
        current = time.time()
        old = current - 60 * 60 * 24 * 21

        for configuration in self.CONFIGURATIONS:
            if (configuration.platform == 'Mac' and configuration.version <= Configuration.version_to_integer('10.13')) \
               or (configuration.platform == 'iOS' and configuration.version <= Configuration.version_to_integer('11')):
                self.database.register_configuration(configuration, branch=None, timestamp=old)
            else:
                self.database.register_configuration(configuration, branch=None, timestamp=current)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_invalid_configuration(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)

        with self.assertRaises(TypeError):
            self.database.register_configuration('invalid object')
        with self.assertRaises(TypeError):
            self.database.register_configuration(Configuration(platform='iOS'))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_no_style_configuration(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)

        self.database.register_configuration(Configuration(
            platform='Mac', version='10.13.0', sdk='17A405', is_simulator=False, architecture='x86_64',
        ))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_configuration_by_platform(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.register_configurations()

        configuration_to_search_for = Configuration(platform='Mac', style='Debug')
        matching_configurations = self.database.search_for_configuration(configuration_to_search_for)
        self.assertEqual(4, len(matching_configurations))
        for config in matching_configurations:
            self.assertEqual(configuration_to_search_for, config)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_configuration_by_platform_with_flavor(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.register_configurations()

        configuration_to_search_for = Configuration(platform='Mac', style='Debug', flavor='wk1')
        matching_configurations = self.database.search_for_configuration(configuration_to_search_for)
        self.assertEqual(2, len(matching_configurations))
        for config in matching_configurations:
            self.assertEqual(configuration_to_search_for, config)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_configuration_by_platform_and_version(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.register_configurations()

        configuration_to_search_for = Configuration(platform='Mac', version='10.13', style='Release')
        matching_configurations = self.database.search_for_configuration(configuration_to_search_for)
        self.assertEqual(2, len(matching_configurations))
        for config in matching_configurations:
            self.assertEqual(configuration_to_search_for, config)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_configuration_by_model(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.register_configurations()
        configuration_to_search_for = Configuration(model='iPhone 8')
        matching_configurations = self.database.search_for_configuration(configuration_to_search_for)
        self.assertEqual(4, len(matching_configurations))
        for config in matching_configurations:
            self.assertEqual(configuration_to_search_for, config)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_configuration_by_architecture(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.register_configurations()

        configuration_to_search_for = Configuration(architecture='x86_64', style='Release')
        matching_configurations = self.database.search_for_configuration(configuration_to_search_for)
        self.assertEqual(6, len(matching_configurations))
        for config in matching_configurations:
            self.assertEqual(configuration_to_search_for, config)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_recent_configurations(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.register_configurations()

        recent_configurations = self.database.search_for_recent_configuration()
        self.assertEqual(12, len(recent_configurations))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_expired_configurations(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.register_configurations()

        recent_configurations = self.database.search_for_configuration()
        self.assertEqual(24, len(recent_configurations))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_recent_configurations_constrained(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.register_configurations()

        configuration_to_search_for = Configuration(architecture='arm64', style='Release')
        matching_configurations = self.database.search_for_recent_configuration(configuration_to_search_for)
        self.assertEqual(1, len(matching_configurations))
        self.assertEqual(matching_configurations[0], configuration_to_search_for)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_recent_configurations_constrained_by_version(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.register_configurations()

        configuration_to_search_for = Configuration(version='12', is_simulator=True)
        matching_configurations = self.database.search_for_recent_configuration(configuration_to_search_for)
        self.assertEqual(3, len(matching_configurations))
        for config in matching_configurations:
            self.assertEqual(configuration_to_search_for, config)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_repopulate_recent(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.register_configurations()

        self.database.redis.flushdb()

        database = ConfigurationContext(redis=self.database.redis, cassandra=self.database.cassandra)
        self.assertEqual(12, len(database.search_for_recent_configuration()))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_partition_by_configuration(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.register_configurations()

        class ExampleModel(ClusteredByConfiguration):
            __table_name__ = 'example_table'
            branch = columns.Text(partition_key=True, required=True)
            index = columns.Integer(primary_key=True, required=True)
            sdk = columns.Text(primary_key=True, required=True)
            json = columns.Text()

        with self.database:
            self.database.cassandra.create_table(ExampleModel)
            for configuration in self.CONFIGURATIONS:
                for i in range(5):
                    self.database.insert_row_with_configuration(ExampleModel.__table_name__, configuration, branch=CommitContext.DEFAULT_BRANCH_KEY, index=i, sdk=configuration.sdk, json=configuration.to_json())

            for configuration in self.CONFIGURATIONS:
                result = self.database.select_from_table_with_configurations(ExampleModel.__table_name__, [configuration], index=2).get(configuration, [])
                self.assertEqual(1, len(result), f'Searching by {configuration} failed, found {len(result)} elements and expected 1')
                self.assertEqual(result[0].json, configuration.to_json())

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_partition_by_partial_configuration(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.register_configurations()

        class ExampleModel(ClusteredByConfiguration):
            __table_name__ = 'example_table'
            branch = columns.Text(partition_key=True, required=True)
            index = columns.Integer(primary_key=True, required=True)
            json = columns.Text()

        with self.database:
            self.database.cassandra.create_table(ExampleModel)
            for configuration in self.CONFIGURATIONS:
                for i in range(5):
                    self.database.insert_row_with_configuration(ExampleModel.__table_name__, configuration, branch=CommitContext.DEFAULT_BRANCH_KEY, index=i, json=configuration.to_json())

            configuration_to_search_for = Configuration(model='iPhone Xs')
            results = self.database.select_from_table_with_configurations(ExampleModel.__table_name__, [configuration_to_search_for], index=4)
            self.assertEqual(2, len(results), f'Searching by {configuration_to_search_for} failed, found {len(results)} elements and expected 2')
            for key, value in results.items():
                self.assertEqual(Configuration.from_json(value[0].json), Configuration.from_json(key.to_json()))
