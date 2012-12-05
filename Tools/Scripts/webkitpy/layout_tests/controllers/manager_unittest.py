# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2010 Gabor Rapcsanyi (rgabor@inf.u-szeged.hu), University of Szeged
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

"""Unit tests for manager.py."""

import sys
import time
import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.layout_tests.controllers.manager import Manager, interpret_test_failures, summarize_results
from webkitpy.layout_tests.models import test_expectations
from webkitpy.layout_tests.models import test_failures
from webkitpy.layout_tests.models import test_results
from webkitpy.layout_tests.models.result_summary import ResultSummary
from webkitpy.thirdparty.mock import Mock
from webkitpy.tool.mocktool import MockOptions


class ManagerTest(unittest.TestCase):
    def test_needs_servers(self):
        def get_manager():
            port = Mock()  # FIXME: Use a tighter mock.
            port.TEST_PATH_SEPARATOR = '/'
            manager = Manager(port, options=MockOptions(http=True, max_locked_shards=1), printer=Mock())
            return manager

        manager = get_manager()
        self.assertFalse(manager.needs_servers(['fast/html']))

        manager = get_manager()
        self.assertTrue(manager.needs_servers(['http/tests/misc']))

    def integration_test_needs_servers(self):
        def get_manager():
            host = MockHost()
            port = host.port_factory.get()
            manager = Manager(port, options=MockOptions(test_list=None, http=True, max_locked_shards=1), printer=Mock())
            return manager

        manager = get_manager()
        self.assertFalse(manager.needs_servers(['fast/html']))

        manager = get_manager()
        self.assertTrue(manager.needs_servers(['http/tests/mime']))

        if sys.platform == 'win32':
            manager = get_manager()
            self.assertFalse(manager.needs_servers(['fast\\html']))

            manager = get_manager()
            self.assertTrue(manager.needs_servers(['http\\tests\\mime']))

    def test_look_for_new_crash_logs(self):
        def get_manager():
            host = MockHost()
            port = host.port_factory.get('test-mac-leopard')
            manager = Manager(port, options=MockOptions(test_list=None, http=True, max_locked_shards=1), printer=Mock())
            return manager
        host = MockHost()
        port = host.port_factory.get('test-mac-leopard')
        tests = ['failures/expected/crash.html']
        expectations = test_expectations.TestExpectations(port, tests)
        rs = ResultSummary(expectations, len(tests))
        manager = get_manager()
        manager._look_for_new_crash_logs(rs, time.time())


class ResultSummaryTest(unittest.TestCase):

    def setUp(self):
        host = MockHost()
        self.port = host.port_factory.get(port_name='test')

    def test_interpret_test_failures(self):
        test_dict = interpret_test_failures([test_failures.FailureImageHashMismatch(diff_percent=0.42)])
        self.assertEqual(test_dict['image_diff_percent'], 0.42)

        test_dict = interpret_test_failures([test_failures.FailureReftestMismatch(self.port.abspath_for_test('foo/reftest-expected.html'))])
        self.assertTrue('image_diff_percent' in test_dict)

        test_dict = interpret_test_failures([test_failures.FailureReftestMismatchDidNotOccur(self.port.abspath_for_test('foo/reftest-expected-mismatch.html'))])
        self.assertEqual(len(test_dict), 0)

        test_dict = interpret_test_failures([test_failures.FailureMissingAudio()])
        self.assertTrue('is_missing_audio' in test_dict)

        test_dict = interpret_test_failures([test_failures.FailureMissingResult()])
        self.assertTrue('is_missing_text' in test_dict)

        test_dict = interpret_test_failures([test_failures.FailureMissingImage()])
        self.assertTrue('is_missing_image' in test_dict)

        test_dict = interpret_test_failures([test_failures.FailureMissingImageHash()])
        self.assertTrue('is_missing_image' in test_dict)

    def get_result(self, test_name, result_type=test_expectations.PASS, run_time=0):
        failures = []
        if result_type == test_expectations.TIMEOUT:
            failures = [test_failures.FailureTimeout()]
        elif result_type == test_expectations.CRASH:
            failures = [test_failures.FailureCrash()]
        return test_results.TestResult(test_name, failures=failures, test_run_time=run_time)

    def get_result_summary(self, port, test_names, expectations_str):
        port.expectations_dict = lambda: {'': expectations_str}
        expectations = test_expectations.TestExpectations(port, test_names)
        return test_names, ResultSummary(expectations, len(test_names)), expectations

    # FIXME: Use this to test more of summarize_results. This was moved from printing_unittest.py.
    def summarized_results(self, port, expected, passing, flaky, extra_tests=[], extra_expectations=None):
        tests = ['passes/text.html', 'failures/expected/timeout.html', 'failures/expected/crash.html', 'failures/expected/wontfix.html']
        if extra_tests:
            tests.extend(extra_tests)

        expectations = ''
        if extra_expectations:
            expectations += extra_expectations

        test_is_slow = False
        paths, rs, exp = self.get_result_summary(port, tests, expectations)
        if expected:
            rs.add(self.get_result('passes/text.html', test_expectations.PASS), expected, test_is_slow)
            rs.add(self.get_result('failures/expected/timeout.html', test_expectations.TIMEOUT), expected, test_is_slow)
            rs.add(self.get_result('failures/expected/crash.html', test_expectations.CRASH), expected, test_is_slow)
        elif passing:
            rs.add(self.get_result('passes/text.html'), expected, test_is_slow)
            rs.add(self.get_result('failures/expected/timeout.html'), expected, test_is_slow)
            rs.add(self.get_result('failures/expected/crash.html'), expected, test_is_slow)
        else:
            rs.add(self.get_result('passes/text.html', test_expectations.TIMEOUT), expected, test_is_slow)
            rs.add(self.get_result('failures/expected/timeout.html', test_expectations.CRASH), expected, test_is_slow)
            rs.add(self.get_result('failures/expected/crash.html', test_expectations.TIMEOUT), expected, test_is_slow)

        for test in extra_tests:
            rs.add(self.get_result(test, test_expectations.CRASH), expected, test_is_slow)

        retry = rs
        if flaky:
            paths, retry, exp = self.get_result_summary(port, tests, expectations)
            retry.add(self.get_result('passes/text.html'), True, test_is_slow)
            retry.add(self.get_result('failures/expected/timeout.html'), True, test_is_slow)
            retry.add(self.get_result('failures/expected/crash.html'), True, test_is_slow)
        unexpected_results = summarize_results(port, exp, rs, retry, only_unexpected=True)
        expected_results = summarize_results(port, exp, rs, retry, only_unexpected=False)
        return expected_results, unexpected_results

    def test_no_svn_revision(self):
        host = MockHost(initialize_scm_by_default=False)
        port = host.port_factory.get('test')
        expected_results, unexpected_results = self.summarized_results(port, expected=False, passing=False, flaky=False)
        self.assertTrue('revision' not in unexpected_results)

    def test_svn_revision(self):
        host = MockHost(initialize_scm_by_default=False)
        port = host.port_factory.get('test')
        port._options.builder_name = 'dummy builder'
        expected_results, unexpected_results = self.summarized_results(port, expected=False, passing=False, flaky=False)
        self.assertNotEquals(unexpected_results['revision'], '')

    def test_summarized_results_wontfix(self):
        host = MockHost()
        port = host.port_factory.get('test')
        port._options.builder_name = 'dummy builder'
        port._filesystem.write_text_file(port._filesystem.join(port.layout_tests_dir(), "failures/expected/wontfix.html"), "Dummy test contents")
        expected_results, unexpected_results = self.summarized_results(port, expected=False, passing=False, flaky=False, extra_tests=['failures/expected/wontfix.html'], extra_expectations='Bug(x) failures/expected/wontfix.html [ WontFix ]\n')
        self.assertTrue(expected_results['tests']['failures']['expected']['wontfix.html']['wontfix'])
