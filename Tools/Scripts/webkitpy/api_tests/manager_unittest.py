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

from webkitpy.api_tests.manager import Manager
from webkitpy.api_tests.test_expectations import PASS, FAIL, CRASH, TIMEOUT
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

    def test_merge_retry_results_flake_is_promoted_to_passed(self):
        from webkitpy.api_tests.runner import Runner

        first_run = {
            'TestA': (Runner.STATUS_FAILED, 'fail output', 1.0),
            'TestB': (Runner.STATUS_PASSED, '', 0.5),
        }
        retry = {'TestA': (Runner.STATUS_PASSED, '', 0.9)}

        final, flakies = Manager._merge_retry_results(first_run, retry)

        self.assertEqual(final['TestA'][0], Runner.STATUS_PASSED)
        self.assertEqual(final['TestB'][0], Runner.STATUS_PASSED)
        self.assertIn('TestA', flakies)
        self.assertEqual(flakies['TestA'][0], Runner.STATUS_FAILED)
        self.assertEqual(flakies['TestA'][1], 'fail output')

    def test_merge_retry_results_consistent_failure_stays_failed(self):
        from webkitpy.api_tests.runner import Runner

        first_run = {'TestA': (Runner.STATUS_FAILED, 'first out', 1.0)}
        retry = {'TestA': (Runner.STATUS_FAILED, 'second out', 1.1)}

        final, flakies = Manager._merge_retry_results(first_run, retry)

        self.assertEqual(final['TestA'][0], Runner.STATUS_FAILED)
        self.assertEqual(final['TestA'][1], 'second out')
        self.assertEqual(flakies, {})

    def test_merge_retry_results_no_retry_data_preserves_first_run(self):
        from webkitpy.api_tests.runner import Runner

        first_run = {'TestA': (Runner.STATUS_CRASHED, 'crashed', 2.0)}
        retry = {}

        final, flakies = Manager._merge_retry_results(first_run, retry)

        self.assertEqual(final['TestA'][0], Runner.STATUS_CRASHED)
        self.assertEqual(flakies, {})
