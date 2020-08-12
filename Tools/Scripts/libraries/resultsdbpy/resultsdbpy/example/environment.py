# Copyright (C) 2020 Apple Inc. All rights reserved.
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

import os
import sys

from cassandra.auth import PlainTextAuthProvider
from fakeredis import FakeStrictRedis
from redis import StrictRedis

sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from resultsdbpy.model.cassandra_context import CassandraContext
from resultsdbpy.model.casserole import CasseroleNodes, CasseroleRedis
from resultsdbpy.model.ci_context import BuildbotURLFactory, BuildbotEightURLFactory
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.model import Model
from resultsdbpy.model.repository import WebKitRepository
from resultsdbpy.model.partitioned_redis import PartitionedRedis

MOCK_CASSANDRA = 'mock'
KEYSPACE = 'results_database_example'


class Environment(object):
    REDIS_PORT = 6379

    def __init__(self, cqleng_allow_schema_management=False):
        self.port = int(os.environ.get('PORT', 5000))

        # The base environment provides defaults for database information, although that can be explicitly overridden
        self.cassandra_server = os.environ.get('CASSANDRA_SERVER', 'localhost')
        self.keyspace = os.environ.get('KEYSPACE', KEYSPACE)

        self.redis_host = os.environ.get('REDIS_HOST', 'localhost')
        self.redis_port = os.environ.get('REDIS_PORT', self.REDIS_PORT)

        self.drop_keyspace = cqleng_allow_schema_management and bool(int(os.environ.get('DROP_KEYSPACE', 0)))
        self.create_keyspace = cqleng_allow_schema_management and bool(int(os.environ.get('CREATE_KEYSPACE', 1)))
        self.cqleng_allow_schema_management = cqleng_allow_schema_management

        os.environ['CQLENG_ALLOW_SCHEMA_MANAGEMENT'] = '1' if self.cqleng_allow_schema_management else '0'

        for key, value in self.__dict__.items():
            if value is None:
                raise EnvironmentError(f'{key.upper()} is not defined in the environment')

        self.redis_url = os.environ.get('REDIS_URL', None)
        self.redis_password = os.environ.get('REDIS_PASSWORD', None)

        self.cassandra_username = None
        self.cassandra_password = None

        if not self.cqleng_allow_schema_management:
            self.cassandra_username = os.environ.get('CASSANDRA_USER')
            self.cassandra_password = os.environ.get('CASSANDRA_USER_PASSWORD')

        if not self.cassandra_username or not self.cassandra_password:
            self.cassandra_username = os.environ.get('CASSANDRA_ADMIN')
            self.cassandra_password = os.environ.get('CASSANDRA_ADMIN_PASSWORD')

    @staticmethod
    def obfuscate_password(password):
        if password:
            return '*' * len(password)
        return '-'

    def __str__(self):
        result = ''
        order = [
            'port',
            'cassandra_server', 'cqleng_allow_schema_management',
            'keyspace', 'drop_keyspace', 'create_keyspace',
            'cassandra_username', 'cassandra_password',
            'redis_url',
            'redis_host', 'redis_port', 'redis_password',
        ]
        for key in self.__dict__.keys():
            if key not in order:
                order.append(key)
        for key in order:
            value = self.__dict__.get(key)
            if value and ('password' in key or 'token' in key or key.endswith('key')):
                value = self.obfuscate_password(value)
            result += f"    {key.upper()}: {value}\n"
        return result[:-1]


class ModelFromEnvironment(Model):
    TTL_SECONDS = Model.TTL_YEAR * 5

    def __init__(self, environment):
        nodes = ['localhost']
        cassandra_class = CassandraContext
        if environment.cassandra_server == MOCK_CASSANDRA:
            cassandra_class = MockCassandraContext

        auth_provider = None
        if environment.cassandra_username and environment.cassandra_password:
            auth_provider = PlainTextAuthProvider(environment.cassandra_username, environment.cassandra_password)

        if environment.drop_keyspace:
            cassandra_class.drop_keyspace(nodes=nodes, keyspace=environment.keyspace, auth_provider=auth_provider)

        cassandra = cassandra_class(
            nodes=nodes,
            keyspace=environment.keyspace,
            auth_provider=auth_provider,
            create_keyspace=environment.create_keyspace,
            replication_map=None,
        )
        if environment.redis_host == 'mock':
            redis = FakeStrictRedis()
        elif environment.redis_url:
            redis = StrictRedis.from_url(environment.redis_url)
        else:
            redis = StrictRedis(
                port=environment.redis_port,
                host=environment.redis_host,
                password=environment.redis_password,
            )
        redis.ping()  # Force a connection with the redis database

        webkit = WebKitRepository()
        super(ModelFromEnvironment, self).__init__(
            redis=redis, cassandra=cassandra,
            repositories=[webkit],
            default_ttl_seconds=self.TTL_SECONDS,
            async_processing=True,
        )


def main():
    environment = Environment(cqleng_allow_schema_management=True)
    print(f'Environment for setup:\n{environment}')
    ModelFromEnvironment(environment)


if __name__ == '__main__':
    main()
