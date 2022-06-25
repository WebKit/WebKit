# Copyright (C) 2010 Google Inc. All rights reserved.
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

from webkitpy.common.host_mock import MockHost
from webkitpy.layout_tests.models import test_expectations
from webkitpy.layout_tests.models import test_failures
from webkitpy.layout_tests.models import test_results
from webkitpy.layout_tests.models import test_run_results
from webkitpy.port.image_diff import ImageDiffResult
from webkitpy.tool.mocktool import MockOptions

from webkitcorepy import OutputCapture
from webkitscmpy import mocks


def get_result(test_name, result_type=test_expectations.PASS, run_time=0):
    failures = []
    if result_type == test_expectations.TIMEOUT:
        failures = [test_failures.FailureTimeout()]
    elif result_type == test_expectations.AUDIO:
        failures = [test_failures.FailureAudioMismatch()]
    elif result_type == test_expectations.CRASH:
        failures = [test_failures.FailureCrash()]
    elif result_type == test_expectations.LEAK:
        failures = [test_failures.FailureDocumentLeak(['http://localhost:8000/failures/expected/leak.html'])]
    return test_results.TestResult(test_name, failures=failures, test_run_time=run_time)


def run_results(port):
    tests = ['passes/text.html', 'failures/expected/timeout.html', 'failures/expected/crash.html', 'failures/expected/hang.html',
             'failures/expected/audio.html', 'failures/expected/leak.html']
    expectations = test_expectations.TestExpectations(port, tests)
    expectations.parse_all_expectations()
    return test_run_results.TestRunResults(expectations, len(tests))


def summarized_results(port, expected, passing, flaky, include_passes=False):
    initial_results = run_results(port)
    if expected:
        initial_results.add(get_result('passes/text.html', test_expectations.PASS), expected)
        initial_results.add(get_result('failures/expected/audio.html', test_expectations.AUDIO), expected)
        initial_results.add(get_result('failures/expected/timeout.html', test_expectations.TIMEOUT), expected)
        initial_results.add(get_result('failures/expected/crash.html', test_expectations.CRASH), expected)

        if port._options.pixel_tests:
            initial_results.add(get_result('failures/expected/pixel-fail.html', test_expectations.IMAGE), expected)
        else:
            initial_results.add(get_result('failures/expected/pixel-fail.html', test_expectations.PASS), expected)

        if port._options.world_leaks:
            initial_results.add(get_result('failures/expected/leak.html', test_expectations.LEAK), expected)
        else:
            initial_results.add(get_result('failures/expected/leak.html', test_expectations.PASS), expected)

    elif passing:
        initial_results.add(get_result('passes/text.html'), expected)
        initial_results.add(get_result('failures/expected/audio.html'), expected)
        initial_results.add(get_result('failures/expected/timeout.html'), expected)
        initial_results.add(get_result('failures/expected/crash.html'), expected)

        if port._options.pixel_tests:
            initial_results.add(get_result('failures/expected/pixel-fail.html'), expected)
        else:
            initial_results.add(get_result('failures/expected/pixel-fail.html', test_expectations.IMAGE), expected)

        if port._options.world_leaks:
            initial_results.add(get_result('failures/expected/leak.html'), expected)
        else:
            initial_results.add(get_result('failures/expected/leak.html', test_expectations.PASS), expected)
    else:
        initial_results.add(get_result('passes/text.html', test_expectations.TIMEOUT), expected)
        initial_results.add(get_result('failures/expected/audio.html', test_expectations.AUDIO), expected)
        initial_results.add(get_result('failures/expected/timeout.html', test_expectations.CRASH), expected)
        initial_results.add(get_result('failures/expected/crash.html', test_expectations.TIMEOUT), expected)
        initial_results.add(get_result('failures/expected/pixel-fail.html', test_expectations.TIMEOUT), expected)
        initial_results.add(get_result('failures/expected/leak.html', test_expectations.CRASH), expected)

        # we only list hang.html here, since normally this is WontFix
        initial_results.add(get_result('failures/expected/hang.html', test_expectations.TIMEOUT), expected)

    if flaky:
        retry_results = run_results(port)
        retry_results.add(get_result('passes/text.html'), True)
        retry_results.add(get_result('failures/expected/timeout.html'), True)
        retry_results.add(get_result('failures/expected/crash.html'), True)
        retry_results.add(get_result('failures/expected/pixel-fail.html'), True)
        retry_results.add(get_result('failures/expected/leak.html'), True)
    else:
        retry_results = None

    return test_run_results.summarize_results(port, {None: initial_results.expectations}, initial_results, retry_results,
        enabled_pixel_tests_in_retry=False, include_passes=include_passes)


class InterpretTestFailuresTest(unittest.TestCase):
    def setUp(self):
        host = MockHost()
        self.port = host.port_factory.get(port_name='test')

    def test_interpret_test_failures(self):
        test_dict = test_run_results._interpret_test_failures([test_failures.FailureImageHashMismatch(ImageDiffResult(passed=False, diff_image=b'', difference=0.42))])
        self.assertEqual(test_dict['image_diff_percent'], 0.42)

        result_fuzzy_data = {'max_difference': 6, 'total_pixels': 50}
        test_dict = test_run_results._interpret_test_failures([test_failures.FailureReftestMismatch(self.port.abspath_for_test('foo/reftest-expected.html'), ImageDiffResult(passed=False, diff_image=b'', difference=100.0, fuzzy_data=result_fuzzy_data))])
        self.assertIn('image_diff_percent', test_dict)
        self.assertIn('image_difference', test_dict)

        test_dict = test_run_results._interpret_test_failures([test_failures.FailureReftestMismatchDidNotOccur(self.port.abspath_for_test('foo/reftest-expected-mismatch.html'))])
        self.assertEqual(len(test_dict), 0)

        test_dict = test_run_results._interpret_test_failures([test_failures.FailureMissingAudio()])
        self.assertIn('is_missing_audio', test_dict)

        test_dict = test_run_results._interpret_test_failures([test_failures.FailureMissingResult()])
        self.assertIn('is_missing_text', test_dict)

        test_dict = test_run_results._interpret_test_failures([test_failures.FailureMissingImage()])
        self.assertIn('is_missing_image', test_dict)

        test_dict = test_run_results._interpret_test_failures([test_failures.FailureMissingImageHash()])
        self.assertIn('is_missing_image', test_dict)


class SummarizedResultsTest(unittest.TestCase):
    def setUp(self):
        host = MockHost(initialize_scm_by_default=False, create_stub_repository_files=True)
        self.port = host.port_factory.get(port_name='test', options=MockOptions(http=True, pixel_tests=False, world_leaks=False))

    def test_no_svn_revision(self):
        summary = summarized_results(self.port, expected=False, passing=False, flaky=False)
        self.assertNotIn('revision', summary)

    def test_svn_revision_exists(self):
        self.port._options.builder_name = 'dummy builder'
        summary = summarized_results(self.port, expected=False, passing=False, flaky=False)
        self.assertNotEqual(summary['revision'], '')

    def test_svn_revision(self):
        with mocks.local.Svn(path='/'), mocks.local.Git():
            self.port._options.builder_name = 'dummy builder'
            summary = summarized_results(self.port, expected=False, passing=False, flaky=False)
            self.assertEqual(summary['revision'], '6')

    def test_svn_revision_git(self):
        with mocks.local.Svn(), mocks.local.Git(path='/', git_svn=True), OutputCapture():
            self.port._options.builder_name = 'dummy builder'
            summary = summarized_results(self.port, expected=False, passing=False, flaky=False)
            self.assertEqual(summary['revision'], '9')

    def test_summarized_results_wontfix(self):
        self.port._options.builder_name = 'dummy builder'
        summary = summarized_results(self.port, expected=False, passing=False, flaky=False)
        self.assertTrue(summary['tests']['failures']['expected']['hang.html']['wontfix'])

    def test_summarized_results_include_passes(self):
        self.port._options.builder_name = 'dummy builder'
        summary = summarized_results(self.port, expected=False, passing=True, flaky=False, include_passes=True)
        self.assertEqual(summary['tests']['passes']['text.html']['expected'], 'PASS')

    def test_summarized_results_world_leaks_disabled(self):
        self.port._options.builder_name = 'dummy builder'
        summary = summarized_results(self.port, expected=False, passing=True, flaky=False, include_passes=True)
        self.assertEqual(summary['tests']['failures']['expected']['leak.html']['expected'], 'PASS')
