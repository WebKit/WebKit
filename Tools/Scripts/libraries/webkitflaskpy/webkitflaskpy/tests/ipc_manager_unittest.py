# Copyright (C) 2023 Apple Inc. All rights reserved.
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

from webkitflaskpy import IPCManager


class IPCManagerUnittest(unittest.TestCase):
    COUNTER = 0
    INCREMENT = 'increment'
    DECREMENT = 'decrement'

    @classmethod
    def increment(cls, value=1):
        if value <= 0:
            raise ValueError('Increment only accepts positive values')
        cls.COUNTER += value

    @classmethod
    def decrement(cls, value=1):
        if value <= 0:
            raise ValueError('Decrement only accepts positive values')
        if value > cls.COUNTER:
            raise ValueError('Trying to decrement more than available in counter')
        cls.COUNTER -= value

    def test_encoding(self):
        self.assertEqual(
            IPCManager.encode(['increment', [1]]),
            ('65634ae55b36be767ee8e26cc3832151', '["increment", [1]]'),
        )
        self.assertEqual(
            IPCManager.encode(dict(a=1, b=2)),
            IPCManager.encode(dict(b=2, a=1)),
        )

    def test_enqueue(self):
        IPCManagerUnittest.COUNTER = 0
        manager = IPCManager()
        manager.register(self.INCREMENT, self.increment)
        manager.register(self.DECREMENT, self.decrement)

        manager.enqueue(self.INCREMENT, 1)
        self.assertEqual(
            [('9ffcb5a7b14e4536e69f6527b1283237', ['increment', [1], {}])],
            list(manager.tasks()),
        )
        manager.enqueue(self.INCREMENT, 2)
        self.assertEqual(2, len(list(manager.tasks())))

        manager.enqueue(self.DECREMENT, 1)
        self.assertEqual(3, len(list(manager.tasks())))

        manager.enqueue(self.DECREMENT, 1)
        self.assertEqual(3, len(list(manager.tasks())))

        self.assertEqual(
            [('831dff37a383fb74ff75d6c7bca3b143', ['decrement', [1], {}])],
            list(manager.tasks(filter=lambda data: data[0] == self.DECREMENT)),
        )

    def test_dequeue(self):
        IPCManagerUnittest.COUNTER = 0
        manager = IPCManager()
        manager.register(self.INCREMENT, self.increment)
        manager.register(self.DECREMENT, self.decrement)

        self.assertEqual(False, manager.dequeue('abc'))

        manager.enqueue(self.INCREMENT, 1)
        self.assertEqual(True, manager.dequeue('9ffcb5a7b14e4536e69f6527b1283237'))
        self.assertEqual(IPCManagerUnittest.COUNTER, 1)

        self.assertEqual(False, manager.dequeue('9ffcb5a7b14e4536e69f6527b1283237'))

    def test_dequeue_all(self):
        IPCManagerUnittest.COUNTER = 0
        manager = IPCManager()
        manager.register(self.INCREMENT, self.increment)
        manager.register(self.DECREMENT, self.decrement)

        self.assertEqual(False, manager.dequeue_all())

        manager.enqueue(self.INCREMENT, 1)
        manager.enqueue(self.INCREMENT, 2)

        self.assertEqual(True, manager.dequeue_all())
        self.assertEqual(IPCManagerUnittest.COUNTER, 3)
        self.assertEqual(False, manager.dequeue_all())

        manager.enqueue(self.DECREMENT, 1)
        manager.enqueue(self.INCREMENT, 3)

        self.assertEqual(True, manager.dequeue_all())
        self.assertEqual(IPCManagerUnittest.COUNTER, 5)
        self.assertEqual(False, manager.dequeue_all())

    def test_retry(self):
        IPCManagerUnittest.COUNTER = 0
        manager = IPCManager()
        manager.register(self.INCREMENT, self.increment)
        manager.register(self.DECREMENT, self.decrement)

        manager.enqueue(self.DECREMENT, 1, retry=0)
        with self.assertRaises(ValueError):
            manager.dequeue_all()
        self.assertEqual(0, len(list(manager.tasks())))
        self.assertEqual(IPCManagerUnittest.COUNTER, 0)

        manager.enqueue(self.DECREMENT, 1, retry=1)
        with self.assertRaises(ValueError):
            manager.dequeue_all()
        self.assertEqual(1, len(list(manager.tasks())))
        IPCManagerUnittest.COUNTER = 1
        self.assertEqual(True, manager.dequeue_all())
        self.assertEqual(IPCManagerUnittest.COUNTER, 0)
