# Copyright (c) 2009 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import unittest

from webkitpy.tool.bot.expectedfailures import ExpectedFailures


class MockResults(object):
    def __init__(self, failing_tests=[], failure_limit=10):
        self._failing_tests = failing_tests
        self._failure_limit_count = failure_limit

    def failure_limit_count(self):
        return self._failure_limit_count

    def failing_tests(self):
        return self._failing_tests


class ExpectedFailuresTest(unittest.TestCase):
    def _assert_can_trust(self, results, can_trust):
        self.assertEquals(ExpectedFailures()._can_trust_results(results), can_trust)

    def test_can_trust_results(self):
        self._assert_can_trust(None, False)
        self._assert_can_trust(MockResults(failing_tests=[], failure_limit=None), False)
        self._assert_can_trust(MockResults(failing_tests=[], failure_limit=10), False)
        self._assert_can_trust(MockResults(failing_tests=[1], failure_limit=None), False)
        self._assert_can_trust(MockResults(failing_tests=[1], failure_limit=2), True)
        self._assert_can_trust(MockResults(failing_tests=[1], failure_limit=1), False)

    def _assert_expected(self, expected_failures, failures, expected):
        self.assertEqual(expected_failures.failures_were_expected(MockResults(failures)), expected)

    def test_failures_were_expected(self):
        failures = ExpectedFailures()
        failures.grow_expected_failures(MockResults(['foo.html']))
        self._assert_expected(failures, ['foo.html'], True)
        self._assert_expected(failures, ['bar.html'], False)
        failures.shrink_expected_failures(MockResults(['baz.html']), False)
        self._assert_expected(failures, ['foo.html'], False)
        self._assert_expected(failures, ['baz.html'], False)

        failures.grow_expected_failures(MockResults(['baz.html']))
        self._assert_expected(failures, ['baz.html'], True)
        failures.shrink_expected_failures(MockResults(), True)
        self._assert_expected(failures, ['baz.html'], False)
