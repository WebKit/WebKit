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
from resultsdbpy.model.partitioned_redis import PartitionedRedis
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase


class PartitionedRedisUnittest(WaitForDockerTestCase):
    PARTITION_1 = 'partition_1'
    PARTITION_2 = 'partition_2'

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis)
    def test_get(self, redis=StrictRedis):
        redis_1 = PartitionedRedis(redis(), self.PARTITION_1)
        redis_2 = PartitionedRedis(redis(), self.PARTITION_2)
        redis_1.set('key', 'value')
        redis_2.set('key', 'other')

        self.assertEqual(redis_1.get('key').decode('utf-8'), 'value')
        self.assertEqual(redis_2.get('key').decode('utf-8'), 'other')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis)
    def test_delete(self, redis=StrictRedis):
        redis_1 = PartitionedRedis(redis(), self.PARTITION_1)
        redis_2 = PartitionedRedis(redis(), self.PARTITION_2)
        redis_1.set('key-a', 'value-a')
        redis_1.set('key-b', 'value-b')
        redis_2.set('key-a', 'other-a')
        redis_2.set('key-b', 'other-b')

        redis_1.delete('key-a', 'key-b')
        self.assertEqual(redis_1.get('key-a'), None)
        self.assertEqual(redis_1.get('key-b'), None)
        self.assertEqual(redis_2.get('key-a').decode('utf-8'), 'other-a')
        self.assertEqual(redis_2.get('key-b').decode('utf-8'), 'other-b')

        redis_2.delete('key-a')
        self.assertEqual(redis_2.get('key-a'), None)
        self.assertEqual(redis_2.get('key-b').decode('utf-8'), 'other-b')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis)
    def test_lock(self, redis=StrictRedis):
        redis_1 = PartitionedRedis(redis(), self.PARTITION_1)
        redis_2 = PartitionedRedis(redis(), self.PARTITION_2)
        with redis_1.lock(name='lock', blocking_timeout=.5):
            with redis_2.lock(name='lock', blocking_timeout=.5):
                pass

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis)
    def test_scan(self, redis=StrictRedis):
        redis_1 = PartitionedRedis(redis(), self.PARTITION_1)
        redis_2 = PartitionedRedis(redis(), self.PARTITION_2)
        redis_1.set('iter-a', 'value-a')
        redis_1.set('iter-b', 'value-b')
        redis_2.set('iter-c', 'value-c')
        redis_2.set('iter-d', 'value-d')

        for key in redis_1.scan_iter('iter*'):
            self.assertIn(key.decode('utf-8'), ['iter-a', 'iter-b'])
        for key in redis_2.scan_iter('iter*'):
            self.assertIn(key.decode('utf-8'), ['iter-c', 'iter-d'])
