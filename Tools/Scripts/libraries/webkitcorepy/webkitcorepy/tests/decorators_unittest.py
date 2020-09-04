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

import time
import unittest

from webkitcorepy import decorators, mocks


class TestMemoize(unittest.TestCase):
    count = 0

    @staticmethod
    @decorators.Memoize()
    def increment_cache():
        TestMemoize.count += 1
        return TestMemoize.count

    @staticmethod
    @decorators.Memoize(cached=False)
    def increment_no_cache():
        TestMemoize.count += 1
        return TestMemoize.count

    @staticmethod
    @decorators.Memoize(timeout=10)
    def increment_timeout_cache():
        TestMemoize.count += 1
        return TestMemoize.count

    @staticmethod
    @decorators.Memoize()
    def increment_and_raise():
        TestMemoize.count += 1
        raise ValueError('EXCEPTION')

    @staticmethod
    @decorators.Memoize()
    def increment_with_args(timeout=None, cached=None):
        return timeout, cached

    def test_cached(self):
        with mocks.Time:
            TestMemoize.count = 0
            TestMemoize.increment_cache.clear()
            self.assertEqual(TestMemoize.increment_cache(), 1)
            self.assertEqual(TestMemoize.increment_cache(), 1)

            self.assertEqual(TestMemoize.increment_cache(cached=False), 2)
            self.assertEqual(TestMemoize.increment_cache(), 2)

            self.assertEqual(TestMemoize.increment_cache(timeout=10), 2)
            time.sleep(15)
            self.assertEqual(TestMemoize.increment_cache(timeout=10), 3)
            self.assertEqual(TestMemoize.increment_cache(), 3)

    def test_not_cached(self):
        TestMemoize.count = 0
        self.assertEqual(TestMemoize.increment_no_cache(), 1)
        self.assertEqual(TestMemoize.increment_no_cache(), 2)

        self.assertEqual(TestMemoize.increment_no_cache(cached=True), 2)

    def test_timeout_cached(self):
        with mocks.Time:
            TestMemoize.count = 0
            TestMemoize.increment_timeout_cache.clear()
            self.assertEqual(TestMemoize.increment_timeout_cache(), 1)
            self.assertEqual(TestMemoize.increment_timeout_cache(), 1)
            time.sleep(5)
            self.assertEqual(TestMemoize.increment_timeout_cache(), 1)
            time.sleep(10)
            self.assertEqual(TestMemoize.increment_timeout_cache(), 2)
            time.sleep(8)
            self.assertEqual(TestMemoize.increment_timeout_cache(timeout=5), 3)

    def test_exception(self):
        with self.assertRaises(ValueError):
            TestMemoize.increment_and_raise()

    def test_conflicting_args(self):
        self.assertEqual(TestMemoize.increment_with_args(
            timeout='timeout',
            cached='cached',
        ), ('timeout', 'cached'))

        self.assertEqual(TestMemoize.increment_with_args(
            timeout='timeout-new',
            cached='cached-new',
        ), ('timeout', 'cached'))
