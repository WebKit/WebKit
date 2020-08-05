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

from fakeredis import FakeStrictRedis
from redis import StrictRedis
from resultsdbpy.model.cassandra_context import CassandraContext
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.mock_model_factory import MockModelFactory
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase


class ModelTest(WaitForDockerTestCase):
    KEYSPACE = 'model_test_keyspace'

    def init_database(self, redis=StrictRedis, cassandra=CassandraContext, async_processing=False):
        cassandra.drop_keyspace(keyspace=self.KEYSPACE)
        self.model = MockModelFactory.create(
            redis=redis(), cassandra=cassandra(keyspace=self.KEYSPACE, create_keyspace=True),
            async_processing=async_processing,
        )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_health(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.assertTrue(self.model.healthy())
        self.assertTrue(self.model.healthy(writable=False))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_no_work(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra, async_processing=True)
        self.assertFalse(self.model.do_work())
