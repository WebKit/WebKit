# Copyright (C) 2021 Apple Inc. All rights reserved.
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

"""Unit tests for manager.py."""

import unittest
from unittest.mock import MagicMock

from webkitpy.api_tests.manager import Manager
from webkitpy.api_tests.test_expectations import PASS, FAIL, CRASH, TIMEOUT
from webkitpy.common.program_status import ProgramStatus
from webkitpy.common.program_status_mock import TTYStringIO, encoded
from webkitexpectationspy.expectations import Expectation


class ManagerTest(unittest.TestCase):
    def test_test_list_from_output(self):
        gtest_list_tests_output = """WKWebViewDisableSelection.
  DoubleClickDoesNotSelectWhenTextInteractionsAreDisabled
  DragDoesNotSelectWhenTextInteractionsAreDisabled
Misc/ValueParametrizedTest.
  ValueParametrizedTestsSupported/CatRed  # GetParam() = (Cat, Red)
  ValueParametrizedTestsSupported/CatGreen  # GetParam() = (Cat, Green)
  ValueParametrizedTestsSupported/DogRed  # GetParam() = (Dog, Red)
  ValueParametrizedTestsSupported/DogGreen  # GetParam() = (Dog, Green)
A.
  B
"""
        expected_tests = [
            "WKWebViewDisableSelection.DoubleClickDoesNotSelectWhenTextInteractionsAreDisabled",
            "WKWebViewDisableSelection.DragDoesNotSelectWhenTextInteractionsAreDisabled",
            "Misc/ValueParametrizedTest.ValueParametrizedTestsSupported/CatRed",
            "Misc/ValueParametrizedTest.ValueParametrizedTestsSupported/CatGreen",
            "Misc/ValueParametrizedTest.ValueParametrizedTestsSupported/DogRed",
            "Misc/ValueParametrizedTest.ValueParametrizedTestsSupported/DogGreen",
            "A.B",
        ]
        got_tests = Manager._test_list_from_output(gtest_list_tests_output)
        self.assertEqual(expected_tests, got_tests)

    def test_test_list_from_output_mixed_gtest_and_swift(self):
        mixed_output = """WKWebViewDisableSelection.
  DoubleClickDoesNotSelectWhenTextInteractionsAreDisabled
TestWebKitAPI.WKWebViewSwiftOverlayTests/evaluateJavaScriptWithNilResponse()
TestWebKitAPI.WKWebViewSwiftOverlayTests/evaluateJavaScriptYieldsExpectedResponse()
"""
        expected_tests = [
            "TestWebKitAPI.WKWebViewDisableSelection.DoubleClickDoesNotSelectWhenTextInteractionsAreDisabled",
            "TestWebKitAPI.WKWebViewSwiftOverlayTests/evaluateJavaScriptWithNilResponse()",
            "TestWebKitAPI.WKWebViewSwiftOverlayTests/evaluateJavaScriptYieldsExpectedResponse()",
        ]
        got_tests = Manager._test_list_from_output(mixed_output, prefix='TestWebKitAPI.')
        self.assertEqual(expected_tests, got_tests)

    def test_find_test_subset_gtest_full_name(self):
        superset = ["TestWebKitAPI.WebKit.SomeTest"]
        self.assertEqual(superset, Manager._find_test_subset(superset, ["TestWebKitAPI.WebKit.SomeTest"]))

    def test_find_test_subset_gtest_suite_and_test(self):
        superset = ["TestWebKitAPI.WebKit.SomeTest"]
        self.assertEqual(superset, Manager._find_test_subset(superset, ["WebKit.SomeTest"]))

    def test_find_test_subset_gtest_binary_and_suite(self):
        superset = ["TestWebKitAPI.WebKit.SomeTest"]
        self.assertEqual(superset, Manager._find_test_subset(superset, ["TestWebKitAPI.WebKit"]))

    def test_find_test_subset_gtest_suite_only(self):
        superset = ["TestWebKitAPI.WebKit.SomeTest"]
        self.assertEqual(superset, Manager._find_test_subset(superset, ["WebKit"]))

    def test_find_test_subset_gtest_wildcard(self):
        superset = ["TestWebKitAPI.WebKit.SomeTest", "TestWebKitAPI.WebKit.OtherTest"]
        self.assertEqual(superset, Manager._find_test_subset(superset, ["WebKit.*"]))

    def test_find_test_subset_swift_full_name(self):
        superset = ["TestWebKitAPI.WKWebViewSwiftOverlayTests/evaluateJavaScriptWithNilResponse()"]
        self.assertEqual(superset, Manager._find_test_subset(superset, ["TestWebKitAPI.WKWebViewSwiftOverlayTests/evaluateJavaScriptWithNilResponse()"]))

    def test_find_test_subset_swift_suite_and_test(self):
        superset = ["TestWebKitAPI.WKWebViewSwiftOverlayTests/evaluateJavaScriptWithNilResponse()"]
        self.assertEqual(superset, Manager._find_test_subset(superset, ["WKWebViewSwiftOverlayTests/evaluateJavaScriptWithNilResponse()"]))

    def test_find_test_subset_swift_binary_and_suite(self):
        superset = ["TestWebKitAPI.WKWebViewSwiftOverlayTests/evaluateJavaScriptWithNilResponse()"]
        self.assertEqual(superset, Manager._find_test_subset(superset, ["TestWebKitAPI.WKWebViewSwiftOverlayTests"]))

    def test_find_test_subset_swift_suite_only(self):
        superset = ["TestWebKitAPI.WKWebViewSwiftOverlayTests/evaluateJavaScriptWithNilResponse()"]
        self.assertEqual(superset, Manager._find_test_subset(superset, ["WKWebViewSwiftOverlayTests"]))

    def test_find_test_subset_swift_wildcard(self):
        superset = [
            "TestWebKitAPI.WKWebViewSwiftOverlayTests/evaluateJavaScriptWithNilResponse()",
            "TestWebKitAPI.WKWebViewSwiftOverlayTests/otherTest()",
        ]
        self.assertEqual(superset, Manager._find_test_subset(superset, ["WKWebViewSwiftOverlayTests/*"]))

    def test_find_test_subset_parentheses_are_literal(self):
        superset = ["TestWebKitAPI.Suite/testFunc()"]
        self.assertEqual(superset, Manager._find_test_subset(superset, ["Suite/testFunc()"]))
        self.assertEqual([], Manager._find_test_subset(superset, ["Suite/testFuncXY"]))

    def test_find_test_subset_no_match(self):
        superset = ["TestWebKitAPI.WebKit.SomeTest"]
        self.assertEqual([], Manager._find_test_subset(superset, ["NonExistent"]))

    def test_parallel_safety_tests_are_collected_with_explicit_arguments(self):
        self.assertEqual(
            Manager._arguments_with_parallel_safety_tests(['TestWTF.Base64'], ['TestWTF.WTF_Vector.Basic TestWTF.WTF_Deque.Basic']),
            ['TestWTF.Base64', 'TestWTF.WTF_Vector.Basic', 'TestWTF.WTF_Deque.Basic'])

    def test_parallel_safety_tests_need_no_arguments_when_collecting_everything(self):
        self.assertEqual(Manager._arguments_with_parallel_safety_tests([], ['TestWTF.WTF_Vector.Basic']), [])

    def test_args_specify_individual_tests(self):
        self.assertTrue(Manager._args_specify_individual_tests(["WebKit.SomeTest"]))
        self.assertTrue(Manager._args_specify_individual_tests(["TestWebKitAPI.WebKit.SomeTest"]))
        self.assertTrue(Manager._args_specify_individual_tests([
            "TestWebKitAPI.WebKit.SomeTest",
            "TestWebKitAPI.WKWebViewSwiftOverlayTests/evaluateJavaScriptWithNilResponse()",
        ]))
        self.assertFalse(Manager._args_specify_individual_tests([]))
        self.assertFalse(Manager._args_specify_individual_tests(["WebKit"]))
        self.assertFalse(Manager._args_specify_individual_tests(["WebKit.SomeTest", "WebKit"]))

    def test_expected_results_for_upload_no_expectation(self):
        self.assertIsNone(Manager._expected_results_for_upload(None))

    def test_expected_results_for_upload_pass(self):
        self.assertIsNone(Manager._expected_results_for_upload(Expectation('TestWebKitAPI.WebKit.SomeTest', expected={PASS})))

    def test_expected_results_for_upload_fail(self):
        self.assertEqual('FAIL', Manager._expected_results_for_upload(Expectation('TestWebKitAPI.WebKit.SomeTest', expected={FAIL})))

    def test_expected_results_for_upload_crash(self):
        self.assertEqual('CRASH', Manager._expected_results_for_upload(Expectation('TestWebKitAPI.WebKit.SomeTest', expected={CRASH})))

    def test_expected_results_for_upload_timeout(self):
        self.assertEqual('TIMEOUT', Manager._expected_results_for_upload(Expectation('TestWebKitAPI.WebKit.SomeTest', expected={TIMEOUT})))

    def test_expected_results_for_upload_flaky(self):
        result = Manager._expected_results_for_upload(Expectation('TestWebKitAPI.WebKit.SomeTest', expected={PASS, FAIL}))
        self.assertEqual({'PASS', 'FAIL'}, set(result.split()))

    def test_program_status_reports_how_the_run_finished(self):
        cases = [
            (Manager.SUCCESS, False, 'state=done:app=run-api-tests'),
            (Manager.FAILED_BUILD_CHECK, False, 'state=error:app=run-api-tests:msg=' + encoded('Build check failed')),
            (Manager.FAILED_COLLECT_TESTS, False, 'state=error:app=run-api-tests:msg=' + encoded('Failed to collect tests')),
            (Manager.FAILED_TESTS, False, 'state=error:app=run-api-tests:msg=' + encoded('Test suite failed')),
            (Manager.FAILED_UPLOAD, False, 'state=error:app=run-api-tests:msg=' + encoded('Upload failed')),
            (Manager.SUCCESS, True, 'state=idle:app=run-api-tests'),
            (Manager.FAILED_TESTS, True, 'state=idle:app=run-api-tests'),
        ]
        for result, interrupted, expected_report in cases:
            with self.subTest(result=result, interrupted=interrupted):
                stream = TTYStringIO()
                manager = Manager(MagicMock(), MagicMock(), MagicMock(), status=ProgramStatus(stream, 'run-api-tests'))
                manager._interrupted = interrupted
                manager._report_finished(result)
                self.assertEqual(stream.program_status_reports(), [expected_report])

    def test_program_status_reports_the_unexpected_failure_count(self):
        stream = TTYStringIO()
        manager = Manager(MagicMock(), MagicMock(), MagicMock(), status=ProgramStatus(stream, 'run-api-tests'))
        manager._unexpected_failure_count = 3
        manager._report_finished(Manager.FAILED_TESTS)
        self.assertEqual(stream.program_status_reports(), ['state=error:app=run-api-tests:msg=' + encoded('3 unexpected failures')])

    def test_write_update_reports_the_phase(self):
        stream = TTYStringIO()
        manager_stream = MagicMock()
        manager = Manager(MagicMock(), MagicMock(), manager_stream, status=ProgramStatus(stream, 'run-api-tests'))
        manager._write_update('Collecting tests ...')
        manager_stream.write_update.assert_called_once_with('Collecting tests ...')
        self.assertEqual(stream.program_status_reports(), ['state=working:app=run-api-tests:msg=' + encoded('Collecting tests ...')])

    def test_run_reports_working_before_the_run_and_finishes_with_its_result(self):
        stream = TTYStringIO()
        manager = Manager(MagicMock(), MagicMock(), MagicMock(), status=ProgramStatus(stream, 'run-api-tests'))
        manager._run = lambda args, json_output: Manager.FAILED_BUILD_CHECK
        self.assertEqual(manager.run([]), Manager.FAILED_BUILD_CHECK)
        self.assertEqual(stream.program_status_reports(), [
            'state=working:app=run-api-tests',
            'state=error:app=run-api-tests:msg=' + encoded('Build check failed'),
        ])
