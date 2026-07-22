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

from webkitpy.api_tests.runner import Runner


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


if __name__ == '__main__':
    unittest.main()
