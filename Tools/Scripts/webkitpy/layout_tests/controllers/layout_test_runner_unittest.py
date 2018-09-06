# Copyright (C) 2012 Google Inc. All rights reserved.
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

import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.layout_tests import run_webkit_tests
from webkitpy.layout_tests.controllers.layout_test_runner import LayoutTestRunner, Sharder, TestRunInterruptedException
from webkitpy.layout_tests.models import test_expectations
from webkitpy.layout_tests.models import test_failures
from webkitpy.layout_tests.models.test_run_results import TestRunResults
from webkitpy.layout_tests.models.test_input import TestInput
from webkitpy.layout_tests.models.test_results import TestResult
from webkitpy.port.test import TestPort


TestExpectations = test_expectations.TestExpectations


class FakePrinter(object):
    num_started = 0
    num_tests = 0

    def print_expected(self, run_results, get_tests_with_result_type):
        pass

    def print_workers_and_shards(self, num_workers, num_shards):
        pass

    def print_started_test(self, test_name):
        pass

    def print_finished_test(self, result, expected, exp_str, got_str):
        pass

    def write(self, msg):
        pass

    def write_update(self, msg):
        pass

    def flush(self):
        pass


class LayoutTestRunnerTests(unittest.TestCase):
    def _runner(self, port=None):
        # FIXME: we shouldn't have to use run_webkit_tests.py to get the options we need.
        options = run_webkit_tests.parse_args(['--platform', 'test-mac-snowleopard'])[0]
        options.child_processes = '1'

        host = MockHost()
        port = port or host.port_factory.get(options.platform, options=options)
        return LayoutTestRunner(options, port, FakePrinter(), port.results_directory(), lambda test_name: False)

    def _run_tests(self, runner, tests):
        test_inputs = [TestInput(test, 6000) for test in tests]
        expectations = TestExpectations(runner._port, tests)
        expectations.parse_all_expectations()
        runner.run_tests(expectations, test_inputs, set(),
            num_workers=1, needs_http=any('http' in test for test in tests), needs_websockets=any(['websocket' in test for test in tests]), needs_web_platform_test_server=any(['imported/w3c' in test for test in tests]), retrying=False)

    def test_interrupt_if_at_failure_limits(self):
        runner = self._runner()
        runner._options.exit_after_n_failures = None
        runner._options.exit_after_n_crashes_or_times = None
        test_names = ['passes/text.html', 'passes/image.html']
        runner._test_inputs = [TestInput(test_name, 6000) for test_name in test_names]

        expectations = TestExpectations(runner._port, test_names)
        expectations.parse_all_expectations()
        run_results = TestRunResults(expectations, len(test_names))
        run_results.unexpected_failures = 100
        run_results.unexpected_crashes = 50
        run_results.unexpected_timeouts = 50
        # No exception when the exit_after* options are None.
        runner._interrupt_if_at_failure_limits(run_results)

        # No exception when we haven't hit the limit yet.
        runner._options.exit_after_n_failures = 101
        runner._options.exit_after_n_crashes_or_timeouts = 101
        runner._interrupt_if_at_failure_limits(run_results)

        # Interrupt if we've exceeded either limit:
        runner._options.exit_after_n_crashes_or_timeouts = 10
        self.assertRaises(TestRunInterruptedException, runner._interrupt_if_at_failure_limits, run_results)
        self.assertEqual(run_results.results_by_name['passes/text.html'].type, test_expectations.SKIP)
        self.assertEqual(run_results.results_by_name['passes/image.html'].type, test_expectations.SKIP)

        runner._options.exit_after_n_crashes_or_timeouts = None
        runner._options.exit_after_n_failures = 10
        exception = self.assertRaises(TestRunInterruptedException, runner._interrupt_if_at_failure_limits, run_results)

    def test_update_summary_with_result(self):
        # Reftests expected to be image mismatch should be respected when pixel_tests=False.
        runner = self._runner()
        runner._options.pixel_tests = False
        runner._options.world_leaks = False
        test = 'failures/expected/reftest.html'
        leak_test = 'failures/expected/leak.html'
        expectations = TestExpectations(runner._port, tests=[test, leak_test])
        expectations.parse_all_expectations()
        runner._expectations = expectations

        run_results = TestRunResults(expectations, 1)
        result = TestResult(test_name=test, failures=[test_failures.FailureReftestMismatchDidNotOccur()], reftest_type=['!='])
        runner._update_summary_with_result(run_results, result)
        self.assertEqual(1, run_results.expected)
        self.assertEqual(0, run_results.unexpected)

        run_results = TestRunResults(expectations, 1)
        result = TestResult(test_name=test, failures=[], reftest_type=['=='])
        runner._update_summary_with_result(run_results, result)
        self.assertEqual(0, run_results.expected)
        self.assertEqual(1, run_results.unexpected)

        run_results = TestRunResults(expectations, 1)
        result = TestResult(test_name=leak_test, failures=[])
        runner._update_summary_with_result(run_results, result)
        self.assertEqual(1, run_results.expected)
        self.assertEqual(0, run_results.unexpected)

    def test_servers_started(self):

        def start_http_server():
            self.http_started = True

        def start_websocket_server():
            self.websocket_started = True

        def start_web_platform_test_server():
            self.web_platform_test_server_started = True

        def stop_http_server():
            self.http_stopped = True

        def stop_websocket_server():
            self.websocket_stopped = True

        def stop_web_platform_test_server():
            self.web_platform_test_server_stopped = True

        def is_http_server_running():
            return self.http_started and not self.http_stopped

        def is_websocket_server_running():
            return self.websocket_started and not self.websocket_stopped

        def is_wpt_server_running():
            return self.websocket_started and not self.web_platform_test_server_stopped

        host = MockHost()
        port = host.port_factory.get('test-mac-leopard')
        port.start_http_server = start_http_server
        port.start_websocket_server = start_websocket_server
        port.start_web_platform_test_server = start_web_platform_test_server
        port.stop_http_server = stop_http_server
        port.stop_websocket_server = stop_websocket_server
        port.stop_web_platform_test_server = stop_web_platform_test_server
        port.is_http_server_running = is_http_server_running
        port.is_websocket_server_running = is_websocket_server_running
        port.is_wpt_server_running = is_wpt_server_running

        self.http_started = self.http_stopped = self.websocket_started = self.websocket_stopped = False
        self.web_platform_test_server_started = self.web_platform_test_server_stopped = False
        runner = self._runner(port=port)
        runner._needs_http = True
        runner._needs_websockets = False
        runner._needs_web_platform_test_server = False
        runner.start_servers()
        self.assertEqual(self.http_started, True)
        self.assertEqual(self.websocket_started, False)
        self.assertEqual(self.web_platform_test_server_started, False)
        runner.stop_servers()
        self.assertEqual(self.http_stopped, True)
        self.assertEqual(self.websocket_stopped, False)
        self.assertEqual(self.web_platform_test_server_stopped, False)

        self.http_started = self.http_stopped = self.websocket_started = self.websocket_stopped = False
        self.web_platform_test_server_started = self.web_platform_test_server_stopped = False
        runner._needs_http = True
        runner._needs_websockets = True
        runner._needs_web_platform_test_server = False
        runner.start_servers()
        self.assertEqual(self.http_started, True)
        self.assertEqual(self.websocket_started, True)
        self.assertEqual(self.web_platform_test_server_started, False)
        runner.stop_servers()
        self.assertEqual(self.http_stopped, True)
        self.assertEqual(self.websocket_stopped, True)
        self.assertEqual(self.web_platform_test_server_stopped, False)

        self.http_started = self.http_stopped = self.websocket_started = self.websocket_stopped = False
        self.web_platform_test_server_started = self.web_platform_test_server_stopped = False
        runner._needs_http = False
        runner._needs_websockets = False
        runner._needs_web_platform_test_server = True
        runner.start_servers()
        self.assertEqual(self.http_started, False)
        self.assertEqual(self.websocket_started, False)
        self.assertEqual(self.web_platform_test_server_started, True)
        runner.stop_servers()
        self.assertEqual(self.http_stopped, False)
        self.assertEqual(self.websocket_stopped, False)
        self.assertEqual(self.web_platform_test_server_stopped, True)

        self.http_started = self.http_stopped = self.websocket_started = self.websocket_stopped = False
        self.web_platform_test_server_started = self.web_platform_test_server_stopped = False
        runner._needs_http = False
        runner._needs_websockets = False
        runner._needs_web_platform_test_server = False
        runner.start_servers()
        self.assertEqual(self.http_started, False)
        self.assertEqual(self.websocket_started, False)
        self.assertEqual(self.web_platform_test_server_started, False)
        runner.stop_servers()
        self.assertEqual(self.http_stopped, False)
        self.assertEqual(self.websocket_stopped, False)
        self.assertEqual(self.web_platform_test_server_stopped, False)


class SharderTests(unittest.TestCase):

    test_list = [
        "http/tests/websocket/tests/unicode.htm",
        "animations/keyframes.html",
        "http/tests/security/view-source-no-refresh.html",
        "http/tests/websocket/tests/websocket-protocol-ignored.html",
        "fast/css/display-none-inline-style-change-crash.html",
        "http/tests/xmlhttprequest/supported-xml-content-types.html",
        "dom/html/level2/html/HTMLAnchorElement03.html",
        "ietestcenter/Javascript/11.1.5_4-4-c-1.html",
        "dom/html/level2/html/HTMLAnchorElement06.html",
    ]

    def get_test_input(self, test_file):
        return TestInput(test_file, needs_servers=(test_file.startswith('http')))

    def get_shards(self, num_workers, fully_parallel, test_list=None):
        port = TestPort(MockSystemHost())
        self.sharder = Sharder(port.split_test)
        test_list = test_list or self.test_list
        return self.sharder.shard_tests([self.get_test_input(test) for test in test_list], num_workers, fully_parallel)

    def assert_shards(self, actual_shards, expected_shard_names):
        self.assertEqual(len(actual_shards), len(expected_shard_names))
        for i, shard in enumerate(actual_shards):
            expected_shard_name, expected_test_names = expected_shard_names[i]
            self.assertEqual(shard.name, expected_shard_name)
            self.assertEqual([test_input.test_name for test_input in shard.test_inputs],
                              expected_test_names)

    def test_shard_by_dir(self):
        result = self.get_shards(num_workers=2, fully_parallel=False)

        self.assert_shards(result,
            [('animations', ['animations/keyframes.html']),
             ('dom/html/level2/html', ['dom/html/level2/html/HTMLAnchorElement03.html',
                                      'dom/html/level2/html/HTMLAnchorElement06.html']),
             ('fast/css', ['fast/css/display-none-inline-style-change-crash.html']),
             ('http/tests/security', ['http/tests/security/view-source-no-refresh.html']),
             ('http/tests/websocket/tests', ['http/tests/websocket/tests/unicode.htm', 'http/tests/websocket/tests/websocket-protocol-ignored.html']),
             ('http/tests/xmlhttprequest', ['http/tests/xmlhttprequest/supported-xml-content-types.html']),
             ('ietestcenter/Javascript', ['ietestcenter/Javascript/11.1.5_4-4-c-1.html'])])

    def test_shard_every_file(self):
        result = self.get_shards(num_workers=2, fully_parallel=True)
        self.assert_shards(result,
            [('.', ['http/tests/websocket/tests/unicode.htm']),
             ('.', ['animations/keyframes.html']),
             ('.', ['http/tests/security/view-source-no-refresh.html']),
             ('.', ['http/tests/websocket/tests/websocket-protocol-ignored.html']),
             ('.', ['fast/css/display-none-inline-style-change-crash.html']),
             ('.', ['http/tests/xmlhttprequest/supported-xml-content-types.html']),
             ('.', ['dom/html/level2/html/HTMLAnchorElement03.html']),
             ('.', ['ietestcenter/Javascript/11.1.5_4-4-c-1.html']),
             ('.', ['dom/html/level2/html/HTMLAnchorElement06.html'])])
