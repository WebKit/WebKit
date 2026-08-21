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

from webkitpy.api_tests.runner import Runner, report_result


class RunnerTest(unittest.TestCase):
    def test_is_disabled_test_detects_disabled_method(self):
        self.assertTrue(Runner.is_disabled_test(
            "TestWebKitAPI.SiteIsolation.DISABLED_PostMessageWithMessagePorts"))

    def test_is_disabled_test_allows_enabled_test(self):
        self.assertFalse(Runner.is_disabled_test(
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

    def test_status_for_output_line_passed(self):
        self.assertEqual(
            Runner.STATUS_PASSED,
            Runner.status_for_output_line("**PASS** SiteIsolation.GrandchildIframe\n"))

    def test_status_for_output_line_failed(self):
        self.assertEqual(
            Runner.STATUS_FAILED,
            Runner.status_for_output_line("**FAIL** SiteIsolation.GrandchildIframe\n"))

    def test_status_for_output_line_skipped_is_disabled(self):
        # A test that called GTEST_SKIP() reports **DISABLED**, because it did not run.
        self.assertEqual(
            Runner.STATUS_DISABLED,
            Runner.status_for_output_line(
                "**DISABLED** UnifiedPDF/EmbeddedPDFFitsToFrame.Test/IFrame_CrossOrigin\n"))

    def test_status_for_output_line_ignores_non_status_output(self):
        self.assertIsNone(Runner.status_for_output_line("Skipping <iframe src=pdf> variants\n"))

    def test_shard_token_lookup_covers_every_status_token(self):
        # The shard parser looks up the exact token, so every token the binaries emit must
        # be present. A missing one falls back to STATUS_FAILED, which is how skipped tests
        # used to be miscounted as failures.
        self.assertEqual(Runner.STATUS_PASSED, Runner.STATUS_FOR_TOKEN.get('**PASS**'))
        self.assertEqual(Runner.STATUS_FAILED, Runner.STATUS_FOR_TOKEN.get('**FAIL**'))
        self.assertEqual(Runner.STATUS_DISABLED, Runner.STATUS_FOR_TOKEN.get('**DISABLED**'))
        self.assertIsNone(Runner.STATUS_FOR_TOKEN.get('**BOGUS**'))

    def test_name_for_status_is_indexed_by_status(self):
        self.assertEqual('Passed', Runner.NAME_FOR_STATUS[Runner.STATUS_PASSED])
        self.assertEqual('Disabled', Runner.NAME_FOR_STATUS[Runner.STATUS_DISABLED])


class _StubPrinter:
    def write_update(self, line):
        pass

    def writeln(self, line):
        pass


class _StubPort:
    def get_option(self, name, default=None):
        return default


class _StubRunner:
    def __init__(self):
        self.printer = _StubPrinter()
        self.port = _StubPort()
        self.results = {}


class ReportResultTest(unittest.TestCase):
    TEST_NAME = "TestWebKitAPI.WebKit.Foo"

    def setUp(self):
        self._saved_instance = Runner.instance
        Runner.instance = _StubRunner()

    def tearDown(self):
        Runner.instance = self._saved_instance

    def _report(self, status):
        report_result('worker/0', self.TEST_NAME, status, '', elapsed=0.1)

    def _recorded_status(self):
        return Runner.instance.results[self.TEST_NAME][0]

    def test_failure_survives_a_later_non_run(self):
        self._report(Runner.STATUS_FAILED)
        self._report(Runner.STATUS_DISABLED)
        self.assertEqual(Runner.STATUS_FAILED, self._recorded_status())

    def test_pass_survives_a_later_non_run(self):
        self._report(Runner.STATUS_PASSED)
        self._report(Runner.STATUS_DISABLED)
        self.assertEqual(Runner.STATUS_PASSED, self._recorded_status())

    def test_failure_replaces_an_earlier_non_run(self):
        self._report(Runner.STATUS_DISABLED)
        self._report(Runner.STATUS_FAILED)
        self.assertEqual(Runner.STATUS_FAILED, self._recorded_status())

    def test_repeated_non_run_stays_disabled(self):
        self._report(Runner.STATUS_DISABLED)
        self._report(Runner.STATUS_DISABLED)
        self.assertEqual(Runner.STATUS_DISABLED, self._recorded_status())

    def test_crash_still_outranks_a_failure(self):
        self._report(Runner.STATUS_FAILED)
        self._report(Runner.STATUS_CRASHED)
        self.assertEqual(Runner.STATUS_CRASHED, self._recorded_status())

    def test_pass_replaces_an_earlier_non_run(self):
        self._report(Runner.STATUS_DISABLED)
        self._report(Runner.STATUS_PASSED)
        self.assertEqual(Runner.STATUS_PASSED, self._recorded_status())

    def test_failure_is_not_downgraded_by_a_later_pass(self):
        self._report(Runner.STATUS_FAILED)
        self._report(Runner.STATUS_PASSED)
        self.assertEqual(Runner.STATUS_FAILED, self._recorded_status())


if __name__ == '__main__':
    unittest.main()
