# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Unit tests for runner.py."""

import unittest
from unittest.mock import MagicMock

from webkitpy.api_tests.runner import EarlyExitException, Runner, report_result


class RunnerTest(unittest.TestCase):
    def test_is_disabled_test_detects_disabled_method(self):
        self.assertTrue(Runner._is_disabled_test(
            "TestWebKitAPI.SiteIsolation.DISABLED_PostMessageWithMessagePorts"))

    def test_is_disabled_test_allows_enabled_test(self):
        self.assertFalse(Runner._is_disabled_test(
            "TestWebKitAPI.SiteIsolation.LoadingCallbacksAndPostMessage"))

    def test_parallel_safety_tests_excludes_disabled(self):
        supplied = [
            "TestWebKitAPI.SiteIsolation.LoadingCallbacksAndPostMessage",
            "TestWebKitAPI.SiteIsolation.DISABLED_PostMessageWithMessagePorts",
            "TestWebKitAPI.SiteIsolation.GrandchildIframe",
        ]
        runnable, disabled = Runner._partition_parallel_safety_tests(supplied)
        self.assertEqual(runnable, [
            "TestWebKitAPI.SiteIsolation.LoadingCallbacksAndPostMessage",
            "TestWebKitAPI.SiteIsolation.GrandchildIframe",
        ])
        self.assertEqual(disabled, [
            "TestWebKitAPI.SiteIsolation.DISABLED_PostMessageWithMessagePorts",
        ])

    def test_partition_keeps_all_enabled_tests(self):
        supplied = [
            "TestWebKitAPI.WebKit.Foo",
            "TestWebKitAPI.WebKit.Bar",
        ]
        runnable, disabled = Runner._partition_parallel_safety_tests(supplied)
        self.assertEqual(runnable, supplied)
        self.assertEqual(disabled, [])


class ReportResultEarlyExitTest(unittest.TestCase):
    def setUp(self):
        self._saved_instance = Runner.instance
        fake = MagicMock()
        fake.port.get_option.return_value = False
        fake.results = {}
        fake.exit_after_n_failures = None
        fake._failure_count = 0
        Runner.instance = fake
        self.fake = fake

    def tearDown(self):
        Runner.instance = self._saved_instance

    def _fail(self, name):
        report_result('worker', name, Runner.STATUS_FAILED, output='err', elapsed=0.1)

    def test_no_early_exit_when_disabled(self):
        self.fake.exit_after_n_failures = None
        for i in range(20):
            self._fail(f'T{i}')
        self.assertEqual(self.fake._failure_count, 20)

    def test_raises_when_threshold_reached(self):
        self.fake.exit_after_n_failures = 3
        self._fail('T1')
        self._fail('T2')
        with self.assertRaises(EarlyExitException) as ctx:
            self._fail('T3')
        self.assertEqual(ctx.exception.failure_count, 3)
        self.assertEqual(self.fake._failure_count, 3)

    def test_crash_and_timeout_count_as_failures(self):
        self.fake.exit_after_n_failures = 3
        report_result('w', 'T1', Runner.STATUS_CRASHED, output='c', elapsed=0.1)
        report_result('w', 'T2', Runner.STATUS_TIMEOUT, output='t', elapsed=0.1)
        with self.assertRaises(EarlyExitException):
            report_result('w', 'T3', Runner.STATUS_FAILED, output='f', elapsed=0.1)

    def test_pass_and_disabled_do_not_count(self):
        self.fake.exit_after_n_failures = 2
        report_result('w', 'P1', Runner.STATUS_PASSED, output='', elapsed=0.1)
        report_result('w', 'D1', Runner.STATUS_DISABLED, output='', elapsed=0.1)
        report_result('w', 'P2', Runner.STATUS_PASSED, output='', elapsed=0.1)
        self.assertEqual(self.fake._failure_count, 0)


class ShardOrderTest(unittest.TestCase):
    """The biggest shard goes out first, so the longest one is not the last thing running alone."""

    def test_biggest_shard_is_dispatched_first(self):
        shards = {
            'TestWebKitAPI.Small': ['a', 'b'],
            'TestWebKitAPI.Biggest': ['c', 'd', 'e', 'f'],
            'TestWebKitAPI.Middle': ['g', 'h', 'i'],
        }
        self.assertEqual(
            [name for name, _ in Runner._shards_longest_first(shards)],
            ['TestWebKitAPI.Biggest', 'TestWebKitAPI.Middle', 'TestWebKitAPI.Small'])

    def test_shards_of_equal_size_keep_a_stable_order(self):
        shards = {'TestWebKitAPI.B': ['1'], 'TestWebKitAPI.A': ['2'], 'TestWebKitAPI.C': ['3']}
        self.assertEqual(
            [name for name, _ in Runner._shards_longest_first(shards)],
            ['TestWebKitAPI.A', 'TestWebKitAPI.B', 'TestWebKitAPI.C'])

    def test_every_shard_is_still_dispatched(self):
        shards = {'a': ['1', '2'], 'b': ['3'], 'c': ['4', '5', '6']}
        ordered = Runner._shards_longest_first(shards)
        self.assertEqual(len(ordered), 3)
        self.assertEqual(sorted(t for _, tests in ordered for t in tests), ['1', '2', '3', '4', '5', '6'])

    def test_no_shards(self):
        self.assertEqual(Runner._shards_longest_first({}), [])


if __name__ == '__main__':
    unittest.main()
