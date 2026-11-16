# Copyright (C) 2021 Apple Inc. All rights reserved.
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

import unittest

from webkitflaskpy import Database


class DatabaseTest(unittest.TestCase):
    PARTITION_1 = 'partition_1:'
    PARTITION_2 = 'partition_2;'
    EMPTY_PARTITION = None

    def test_get(self):
        redis_1 = Database(partition=self.PARTITION_1)
        redis_2 = Database(partition=self.PARTITION_2)
        redis_3 = Database(partition=self.EMPTY_PARTITION)
        redis_1.set('key', 'value')
        redis_2.set('key', 'other')
        redis_3.set('key', 'empty')

        self.assertEqual(redis_1.get('key').decode('utf-8'), 'value')
        self.assertEqual(redis_2.get('key').decode('utf-8'), 'other')
        self.assertEqual(redis_3.get('key').decode('utf-8'), 'empty')

    def test_delete(self):
        redis_1 = Database(partition=self.PARTITION_1)
        redis_2 = Database(partition=self.PARTITION_2)
        redis_3 = Database(partition=self.EMPTY_PARTITION)

        redis_1.set('key-a', 'value-a')
        redis_1.set('key-b', 'value-b')
        redis_2.set('key-a', 'other-a')
        redis_2.set('key-b', 'other-b')
        redis_3.set('key-a', 'empty-a')
        redis_3.set('key-b', 'empty-b')

        redis_1.delete('key-a', 'key-b')
        self.assertEqual(redis_1.get('key-a'), None)
        self.assertEqual(redis_1.get('key-b'), None)

        self.assertEqual(redis_2.get('key-a').decode('utf-8'), 'other-a')
        self.assertEqual(redis_2.get('key-b').decode('utf-8'), 'other-b')
        self.assertEqual(redis_3.get('key-a').decode('utf-8'), 'empty-a')
        self.assertEqual(redis_3.get('key-b').decode('utf-8'), 'empty-b')

        redis_2.delete('key-a')
        self.assertEqual(redis_2.get('key-a'), None)
        self.assertEqual(redis_2.get('key-b').decode('utf-8'), 'other-b')

        redis_3.delete('key-a')
        self.assertEqual(redis_3.get('key-a'), None)
        self.assertEqual(redis_3.get('key-b').decode('utf-8'), 'empty-b')

    def test_lock(self):
        redis_1 = Database(partition=self.PARTITION_1)
        redis_2 = Database(partition=self.PARTITION_2)
        redis_3 = Database(partition=self.EMPTY_PARTITION)
        with redis_1.lock(name='lock', blocking_timeout=.5):
            with redis_2.lock(name='lock', blocking_timeout=.5):
                with redis_3.lock(name='lock', blocking_timeout=.5):
                    pass

    def test_scan(self):
        redis_1 = Database(partition=self.PARTITION_1)
        redis_2 = Database(partition=self.PARTITION_2)
        redis_3 = Database(partition=self.EMPTY_PARTITION)
        redis_1.set('iter-a', 'value-a')
        redis_1.set('iter-b', 'value-b')
        redis_2.set('iter-c', 'value-c')
        redis_2.set('iter-d', 'value-d')
        redis_3.set('iter-e', 'value-e')
        redis_3.set('iter-f', 'value-f')

        for key in redis_1.scan_iter('iter*'):
            self.assertIn(key.decode('utf-8'), ['iter-a', 'iter-b'])
        for key in redis_2.scan_iter('iter*'):
            self.assertIn(key.decode('utf-8'), ['iter-c', 'iter-d'])
        for key in redis_3.scan_iter('iter*'):
            self.assertIn(key.decode('utf-8'), ['iter-e', 'iter-f'])
