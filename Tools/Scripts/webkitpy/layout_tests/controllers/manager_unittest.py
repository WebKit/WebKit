#!/usr/bin/python
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

import StringIO
import sys
import time
import unittest

from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system import outputcapture
from webkitpy.thirdparty.mock import Mock
from webkitpy import layout_tests
from webkitpy.layout_tests.port import port_testcase

from webkitpy import layout_tests
from webkitpy.layout_tests import run_webkit_tests
from webkitpy.layout_tests.controllers import manager
from webkitpy.layout_tests.controllers.manager import interpret_test_failures,  Manager, TestRunInterruptedException
from webkitpy.layout_tests.models import test_expectations
from webkitpy.layout_tests.models import test_failures
from webkitpy.layout_tests.models import test_results
from webkitpy.layout_tests.models.result_summary import ResultSummary
from webkitpy.layout_tests.models.test_expectations import TestExpectations
from webkitpy.layout_tests.models.test_results import TestResult
from webkitpy.layout_tests.models.test_input import TestInput
from webkitpy.layout_tests.views import printing
from webkitpy.tool.mocktool import MockOptions
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.host_mock import MockHost


class LockCheckingManager(Manager):
    def __init__(self, port, options, printer, tester, http_lock):
        super(LockCheckingManager, self).__init__(port, options, printer)
        self._finished_list_called = False
        self._tester = tester
        self._should_have_http_lock = http_lock

    def handle_finished_list(self, source, list_name, num_tests, elapsed_time):
        if not self._finished_list_called:
            self._tester.assertEquals(list_name, 'locked_tests')
            self._tester.assertTrue(self._remaining_locked_shards)
            self._tester.assertTrue(self._has_http_lock is self._should_have_http_lock)

        super(LockCheckingManager, self).handle_finished_list(source, list_name, num_tests, elapsed_time)

        if not self._finished_list_called:
            self._tester.assertEquals(self._remaining_locked_shards, [])
            self._tester.assertFalse(self._has_http_lock)
            self._finished_list_called = True


class ManagerTest(unittest.TestCase):
    def get_options(self):
        return MockOptions(pixel_tests=False, new_baseline=False, time_out_ms=6000, slow_time_out_ms=30000, worker_model='inline', max_locked_shards=1)

    def test_http_locking(tester):
        options, args = run_webkit_tests.parse_args(['--platform=test', '--print=nothing', 'http/tests/passes', 'passes'])
        host = MockHost()
        port = host.port_factory.get(port_name=options.platform, options=options)
        run_webkit_tests._set_up_derived_options(port, options)
        printer = printing.Printer(port, options, StringIO.StringIO(), StringIO.StringIO())
        manager = LockCheckingManager(port, options, printer, tester, True)
        num_unexpected_results = manager.run(args)
        printer.cleanup()
        tester.assertEquals(num_unexpected_results, 0)

    def test_perf_locking(tester):
        options, args = run_webkit_tests.parse_args(['--platform=test', '--print=nothing', '--no-http', 'passes', 'perf/'])
        host = MockHost()
        port = host.port_factory.get(port_name=options.platform, options=options)
        run_webkit_tests._set_up_derived_options(port, options)
        printer = printing.Printer(port, options, StringIO.StringIO(), StringIO.StringIO())
        manager = LockCheckingManager(port, options, printer, tester, False)
        num_unexpected_results = manager.run(args)
        printer.cleanup()
        tester.assertEquals(num_unexpected_results, 0)

    def test_interrupt_if_at_failure_limits(self):
        port = Mock()  # FIXME: This should be a tighter mock.
        port.TEST_PATH_SEPARATOR = '/'
        port._filesystem = MockFileSystem()
        manager = Manager(port=port, options=MockOptions(max_locked_shards=1), printer=Mock())

        manager._options = MockOptions(exit_after_n_failures=None, exit_after_n_crashes_or_timeouts=None, max_locked_shards=1)
        manager._test_names = ['foo/bar.html', 'baz.html']
        manager._test_is_slow = lambda test_name: False

        result_summary = ResultSummary(Mock(), manager._test_names, 1, set())
        result_summary.unexpected_failures = 100
        result_summary.unexpected_crashes = 50
        result_summary.unexpected_timeouts = 50
        # No exception when the exit_after* options are None.
        manager._interrupt_if_at_failure_limits(result_summary)

        # No exception when we haven't hit the limit yet.
        manager._options.exit_after_n_failures = 101
        manager._options.exit_after_n_crashes_or_timeouts = 101
        manager._interrupt_if_at_failure_limits(result_summary)

        # Interrupt if we've exceeded either limit:
        manager._options.exit_after_n_crashes_or_timeouts = 10
        self.assertRaises(TestRunInterruptedException, manager._interrupt_if_at_failure_limits, result_summary)

        self.assertEquals(result_summary.results['foo/bar.html'].type, test_expectations.SKIP)
        self.assertEquals(result_summary.results['baz.html'].type, test_expectations.SKIP)

        manager._options.exit_after_n_crashes_or_timeouts = None
        manager._options.exit_after_n_failures = 10
        exception = self.assertRaises(TestRunInterruptedException, manager._interrupt_if_at_failure_limits, result_summary)

    def test_update_summary_with_result(self):
        host = MockHost()
        port = host.port_factory.get('test-win-xp')
        test = 'failures/expected/reftest.html'
        port.expectations_dict = lambda: {'': 'WONTFIX : failures/expected/reftest.html = IMAGE'}
        expectations = TestExpectations(port, tests=[test])
        # Reftests expected to be image mismatch should be respected when pixel_tests=False.
        manager = Manager(port=port, options=MockOptions(pixel_tests=False, exit_after_n_failures=None, exit_after_n_crashes_or_timeouts=None, max_locked_shards=1), printer=Mock())
        manager._expectations = expectations
        result_summary = ResultSummary(expectations, [test], 1, set())
        result = TestResult(test_name=test, failures=[test_failures.FailureReftestMismatchDidNotOccur()])
        manager._update_summary_with_result(result_summary, result)
        self.assertEquals(1, result_summary.expected)
        self.assertEquals(0, result_summary.unexpected)

    def test_needs_servers(self):
        def get_manager_with_tests(test_names):
            port = Mock()  # FIXME: Use a tighter mock.
            port.TEST_PATH_SEPARATOR = '/'
            manager = Manager(port, options=MockOptions(http=True, max_locked_shards=1), printer=Mock())
            manager._test_names = test_names
            return manager

        manager = get_manager_with_tests(['fast/html'])
        self.assertFalse(manager.needs_servers())

        manager = get_manager_with_tests(['http/tests/misc'])
        self.assertTrue(manager.needs_servers())

    def integration_test_needs_servers(self):
        def get_manager_with_tests(test_names):
            host = MockHost()
            port = host.port_factory.get()
            manager = Manager(port, options=MockOptions(test_list=None, http=True, max_locked_shards=1), printer=Mock())
            manager._collect_tests(test_names)
            return manager

        manager = get_manager_with_tests(['fast/html'])
        self.assertFalse(manager.needs_servers())

        manager = get_manager_with_tests(['http/tests/mime'])
        self.assertTrue(manager.needs_servers())

        if sys.platform == 'win32':
            manager = get_manager_with_tests(['fast\\html'])
            self.assertFalse(manager.needs_servers())

            manager = get_manager_with_tests(['http\\tests\\mime'])
            self.assertTrue(manager.needs_servers())

    def test_look_for_new_crash_logs(self):
        def get_manager_with_tests(test_names):
            host = MockHost()
            port = host.port_factory.get('test-mac-leopard')
            manager = Manager(port, options=MockOptions(test_list=None, http=True, max_locked_shards=1), printer=Mock())
            manager._collect_tests(test_names)
            return manager
        host = MockHost()
        port = host.port_factory.get('test-mac-leopard')
        tests = ['failures/expected/crash.html']
        expectations = test_expectations.TestExpectations(port, tests)
        rs = ResultSummary(expectations, tests, 1, set())
        manager = get_manager_with_tests(tests)
        manager._look_for_new_crash_logs(rs, time.time())

    def test_servers_started(self):

        def start_http_server(number_of_servers=None):
            self.http_started = True

        def start_websocket_server():
            self.websocket_started = True

        def stop_http_server():
            self.http_stopped = True

        def stop_websocket_server():
            self.websocket_stopped = True

        host = MockHost()
        port = host.port_factory.get('test-mac-leopard')
        port.start_http_server = start_http_server
        port.start_websocket_server = start_websocket_server
        port.stop_http_server = stop_http_server
        port.stop_websocket_server = stop_websocket_server

        self.http_started = self.http_stopped = self.websocket_started = self.websocket_stopped = False
        manager = Manager(port=port, options=MockOptions(http=True, max_locked_shards=1), printer=Mock())
        manager._test_names = ['http/tests/pass.txt']
        manager.start_servers_with_lock(number_of_servers=4)
        self.assertEquals(self.http_started, True)
        self.assertEquals(self.websocket_started, False)
        manager.stop_servers_with_lock()
        self.assertEquals(self.http_stopped, True)
        self.assertEquals(self.websocket_stopped, False)

        self.http_started = self.http_stopped = self.websocket_started = self.websocket_stopped = False
        manager = Manager(port=port, options=MockOptions(http=True, max_locked_shards=1), printer=Mock())
        manager._test_names = ['websocket/pass.txt']
        manager.start_servers_with_lock(number_of_servers=4)
        self.assertEquals(self.http_started, True)
        self.assertEquals(self.websocket_started, True)
        manager.stop_servers_with_lock()
        self.assertEquals(self.http_stopped, True)
        self.assertEquals(self.websocket_stopped, True)

        self.http_started = self.http_stopped = self.websocket_started = self.websocket_stopped = False
        manager = Manager(port=port, options=MockOptions(http=True, max_locked_shards=1), printer=Mock())
        manager._test_names = ['perf/foo/test.html']
        manager.start_servers_with_lock(number_of_servers=4)
        self.assertEquals(self.http_started, False)
        self.assertEquals(self.websocket_started, False)
        manager.stop_servers_with_lock()
        self.assertEquals(self.http_stopped, False)
        self.assertEquals(self.websocket_stopped, False)



class ResultSummaryTest(unittest.TestCase):

    def setUp(self):
        host = MockHost()
        self.port = host.port_factory.get(port_name='test')

    def test_interpret_test_failures(self):
        test_dict = interpret_test_failures(self.port, 'foo/reftest.html',
            [test_failures.FailureReftestMismatch(self.port.abspath_for_test('foo/reftest-expected.html'))])
        self.assertTrue('is_reftest' in test_dict)
        self.assertFalse('is_mismatch_reftest' in test_dict)

        test_dict = interpret_test_failures(self.port, 'foo/reftest.html',
            [test_failures.FailureReftestMismatch(self.port.abspath_for_test('foo/common.html'))])
        self.assertTrue('is_reftest' in test_dict)
        self.assertFalse('is_mismatch_reftest' in test_dict)

        test_dict = interpret_test_failures(self.port, 'foo/reftest.html',
            [test_failures.FailureReftestMismatchDidNotOccur(self.port.abspath_for_test('foo/reftest-expected-mismatch.html'))])
        self.assertFalse('is_reftest' in test_dict)
        self.assertTrue(test_dict['is_mismatch_reftest'])

        test_dict = interpret_test_failures(self.port, 'foo/reftest.html',
            [test_failures.FailureReftestMismatchDidNotOccur(self.port.abspath_for_test('foo/common.html'))])
        self.assertFalse('is_reftest' in test_dict)
        self.assertTrue(test_dict['is_mismatch_reftest'])

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
        return test_names, ResultSummary(expectations, test_names, 1, set()), expectations

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
        unexpected_results = manager.summarize_results(port, exp, rs, retry, test_timings={}, only_unexpected=True, interrupted=False)
        expected_results = manager.summarize_results(port, exp, rs, retry, test_timings={}, only_unexpected=False, interrupted=False)
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
        expected_results, unexpected_results = self.summarized_results(port, expected=False, passing=False, flaky=False, extra_tests=['failures/expected/wontfix.html'], extra_expectations='BUGX WONTFIX : failures/expected/wontfix.html = TEXT\n')
        self.assertTrue(expected_results['tests']['failures']['expected']['wontfix.html']['wontfix'])

if __name__ == '__main__':
    port_testcase.main()
