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

import pickle
import unittest

from unittest.mock import patch

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.layout_tests import run_webkit_tests
from webkitpy.layout_tests.controllers import layout_test_runner
from webkitpy.layout_tests.controllers.layout_test_runner import (
    LayoutTestRunner,
    Sharder,
    TestRunInterruptedException,
    TestShard,
    UnclaimedShard,
    Worker,
)
from webkitpy.layout_tests.models import test_expectations, test_failures
from webkitpy.layout_tests.models.test import Test
from webkitpy.layout_tests.models.test_input import TestInput
from webkitpy.layout_tests.models.test_results import TestResult
from webkitpy.layout_tests.models.test_run_results import TestRunResults
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
        return LayoutTestRunner(options, port, FakePrinter(), port.results_directory())

    def _run_tests(self, runner, tests):
        test_inputs = [TestInput(Test(test), 6000) for test in tests]
        expectations = TestExpectations(runner._port, tests)
        expectations.parse_all_expectations()
        runner.run_tests(expectations, test_inputs,
            num_workers=1, needs_http=any('http' in test for test in tests), needs_websockets=any(['websocket' in test for test in tests]), needs_web_platform_test_server=any(['imported/w3c' in test for test in tests]), retrying=False)

    def test_interrupt_if_at_failure_limits(self):
        runner = self._runner()
        runner._options.exit_after_n_failures = None
        runner._options.exit_after_n_crashes_or_times = None
        test_names = ['passes/text.html', 'passes/image.html']
        runner._test_inputs = [TestInput(Test(test_name), 6000) for test_name in test_names]

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
        self.assertRaises(TestRunInterruptedException, runner._interrupt_if_at_failure_limits, run_results)

    def test_update_summary_with_result(self):
        # Reftests expected to be image mismatch should be respected when pixel_tests=False.
        runner = self._runner()
        runner._options.pixel_tests = False
        runner._options.world_leaks = False
        test = 'failures/expected/reftest.html'
        leak_test = 'failures/expected/leak.html'
        timeout_test = 'failures/expected/timeout.html'
        expectations = TestExpectations(runner._port, tests=[test, leak_test, timeout_test])
        expectations.parse_all_expectations()
        runner._expectations = expectations

        runner._current_run_results = TestRunResults(expectations, 1)
        result = TestResult(test, failures=[test_failures.FailureReftestMismatchDidNotOccur()], reftest_type=['!='])
        runner.update_summary_with_result(result)
        self.assertEqual(1, runner._current_run_results.expected)
        self.assertEqual(0, runner._current_run_results.unexpected)

        runner._current_run_results = TestRunResults(expectations, 1)
        result = TestResult(test, failures=[], reftest_type=['=='])
        runner.update_summary_with_result(result)
        self.assertEqual(0, runner._current_run_results.expected)
        self.assertEqual(1, runner._current_run_results.unexpected)

        runner._current_run_results = TestRunResults(expectations, 1)
        result = TestResult(leak_test, failures=[])
        runner.update_summary_with_result(result)
        self.assertEqual(1, runner._current_run_results.expected)
        self.assertEqual(0, runner._current_run_results.unexpected)

        runner._current_run_results = TestRunResults(expectations, 3)
        result = TestResult(timeout_test, failures=[])
        runner.update_summary_with_result(result)
        self.assertEqual(0, runner._current_run_results.expected)
        self.assertEqual(1, runner._current_run_results.unexpected)
        result = TestResult(timeout_test, failures=[test_failures.FailureTextMismatch()])
        runner.update_summary_with_result(result)
        self.assertEqual(0, runner._current_run_results.expected)
        self.assertEqual(2, runner._current_run_results.unexpected)
        result = TestResult(timeout_test, failures=[])
        runner.update_summary_with_result(result)
        self.assertEqual(0, runner._current_run_results.expected)
        self.assertEqual(3, runner._current_run_results.unexpected)
        result = TestResult(timeout_test, failures=[])
        runner.update_summary_with_result(result)
        self.assertEqual(0, runner._current_run_results.expected)
        self.assertEqual(4, runner._current_run_results.unexpected)

    def test_servers_started(self):

        def start_http_server(additional_dirs=None):
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


class UnusableDeviceTests(unittest.TestCase):
    """A device can go away while a shard is running. The tests it was given have to reach a device that is still
    working, rather than being recorded as failures they did not have."""

    class FakeProcess(object):
        name = 'worker/0'
        working = True

    def _worker(self, port):
        worker = Worker(port, port.results_directory())
        Worker.instance = worker
        self.addCleanup(setattr, Worker, 'instance', None)
        return worker

    def _port_with_usable(self, usable):
        options = run_webkit_tests.parse_args(['--platform', 'test-mac-snowleopard'])[0]
        port = MockHost().port_factory.get(options.platform, options=options)
        port.target_host_is_usable = lambda worker_number=None, force_update=False: usable
        return port

    def setUp(self):
        patcher = patch.object(layout_test_runner.TaskPool, 'Process', self.FakeProcess)
        patcher.start()
        self.addCleanup(patcher.stop)

    def test_shard_is_returned_when_device_is_unusable(self):
        port = self._port_with_usable(False)
        worker = self._worker(port)
        shard = TestShard('shard', [TestInput(Test('fast/a.html'), 6000), TestInput(Test('fast/b.html'), 6000)])

        ran = []
        worker.run_test = lambda test_input, shard_name: ran.append(test_input)
        result = worker.run_tests(shard)

        self.assertIsInstance(result, UnclaimedShard)
        self.assertEqual(len(result.tests), 2)
        self.assertEqual(ran, [], 'no test should run against an unusable device')

    def test_shard_runs_normally_when_device_is_usable(self):
        port = self._port_with_usable(True)
        worker = self._worker(port)
        shard = TestShard('shard', [TestInput(Test('fast/a.html'), 6000), TestInput(Test('fast/b.html'), 6000)])

        ran = []
        worker.run_test = lambda test_input, shard_name: ran.append(test_input)
        result = worker.run_tests(shard)

        self.assertNotIsInstance(result, UnclaimedShard)
        self.assertEqual(len(ran), 2)

    def test_shard_is_split_when_device_dies_partway_through(self):
        port = self._port_with_usable(True)
        worker = self._worker(port)
        shard = TestShard('shard', [TestInput(Test('fast/{}.html'.format(name)), 6000) for name in ('a', 'b', 'c', 'd')])

        ran = []

        def run_test(test_input, shard_name):
            ran.append(test_input)
            # The device goes away after the second test, taking the driver with it, which is how a lost device
            # announces itself: the test crashes and _clean_up_after_test kills the driver.
            if len(ran) == 2:
                port.target_host_is_usable = lambda worker_number=None, force_update=False: False
                worker._driver_died = True

        worker.run_test = run_test
        result = worker.run_tests(shard)

        self.assertIsInstance(result, UnclaimedShard)
        self.assertEqual(len(ran), 2, 'the run should stop as soon as the device is gone')
        self.assertEqual(
            [test_input.test_name for test_input in result.tests],
            ['fast/c.html', 'fast/d.html'],
            'only the tests that did not run should come back')

    def test_dead_driver_alone_does_not_return_the_shard(self):
        port = self._port_with_usable(True)
        worker = self._worker(port)
        shard = TestShard('shard', [TestInput(Test('fast/a.html'), 6000), TestInput(Test('fast/b.html'), 6000)])

        ran = []

        def run_test(test_input, shard_name):
            ran.append(test_input)
            worker._driver_died = True

        worker.run_test = run_test
        result = worker.run_tests(shard)

        self.assertNotIsInstance(result, UnclaimedShard)
        self.assertEqual(len(ran), 2, 'a test that crashes on a healthy device must not end the shard')

    def test_unclaimed_shard_reports_how_much_was_left(self):
        self.assertEqual(len(UnclaimedShard([1, 2, 3]).tests), 3)
        self.assertIn('3', repr(UnclaimedShard([1, 2, 3])))

    def test_unclaimed_shard_survives_pickling(self):
        # The worker returns this across a process boundary, so it has to pickle with its test inputs intact.
        shard = UnclaimedShard([TestInput(Test('fast/a.html'), 6000)])
        restored = pickle.loads(pickle.dumps(shard))
        self.assertEqual([test_input.test_name for test_input in restored.tests], ['fast/a.html'])


class AbandoningShardsTests(unittest.TestCase):
    """A shard is only given up on when no device will run any of it. One that keeps making progress keeps going
    around, however many devices it has to pass through."""

    def _runner(self, usable=True):
        options = run_webkit_tests.parse_args(['--platform', 'test-mac-snowleopard'])[0]
        options.child_processes = '1'
        host = MockHost()
        port = host.port_factory.get(options.platform, options=options)
        port.target_host_is_usable = lambda worker_number=None, force_update=False: usable
        port.has_usable_device = lambda: usable
        port.expects_more_devices = lambda: False
        return LayoutTestRunner(options, port, FakePrinter(), port.results_directory())

    def test_shard_that_keeps_shrinking_is_never_abandoned(self):
        runner = self._runner()
        remaining = 40
        fruitless_attempts = 0
        rounds = 0
        while remaining > 1:
            dispatched, remaining = remaining, remaining - 1
            fruitless_attempts = runner._port.fruitless_attempts_after(dispatched, remaining, fruitless_attempts)
            self.assertFalse(
                runner._port.should_abandon_shard(fruitless_attempts, 4),
                'a shard that ran a test must not be given up on')
            rounds += 1
        self.assertEqual(rounds, 39, 'every round should have made progress')

    def test_shard_no_device_will_run_is_eventually_abandoned(self):
        runner = self._runner()
        fruitless_attempts = 0
        for attempt in range(1, 5):
            fruitless_attempts = runner._port.fruitless_attempts_after(8, 8, fruitless_attempts)
            self.assertEqual(fruitless_attempts, attempt)
        self.assertTrue(runner._port.should_abandon_shard(fruitless_attempts, 4))

    def test_every_worker_gets_a_turn_before_giving_up(self):
        runner = self._runner()
        # A shard must outlast one bad device per worker, so a large pool has to try harder than the floor of 3.
        self.assertFalse(runner._port.should_abandon_shard(5, 25))
        self.assertTrue(runner._port.should_abandon_shard(25, 25))
        self.assertTrue(runner._port.should_abandon_shard(3, 1))

    def test_shard_is_abandoned_when_no_device_is_usable(self):
        runner = self._runner(usable=False)
        self.assertTrue(runner._port.should_abandon_shard(0, 4))


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
        return TestInput(Test(test_file), needs_servers=(test_file.startswith('http')))

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

        # Shards are dispatched biggest first so that a large one is not left running alone at the end of a run.
        self.assert_shards(result, [
            ('dom/html/level2/html', ['dom/html/level2/html/HTMLAnchorElement03.html', 'dom/html/level2/html/HTMLAnchorElement06.html']),
            ('http/tests/websocket/tests', ['http/tests/websocket/tests/unicode.htm', 'http/tests/websocket/tests/websocket-protocol-ignored.html']),
            ('animations', ['animations/keyframes.html']),
            ('fast/css', ['fast/css/display-none-inline-style-change-crash.html']),
            ('http/tests/security', ['http/tests/security/view-source-no-refresh.html']),
            ('http/tests/xmlhttprequest', ['http/tests/xmlhttprequest/supported-xml-content-types.html']),
            ('ietestcenter/Javascript', ['ietestcenter/Javascript/11.1.5_4-4-c-1.html'])])

    def test_shards_of_equal_size_stay_in_directory_order(self):
        result = self.get_shards(num_workers=2, fully_parallel=False)
        single_test_shards = [shard.name for shard in result if len(shard.test_inputs) == 1]
        self.assertEqual(sorted(single_test_shards), single_test_shards)

    def test_shard_every_file(self):
        result = self.get_shards(num_workers=2, fully_parallel=True)
        self.assert_shards(result, [
            ('.', ['http/tests/websocket/tests/unicode.htm']),
            ('.', ['animations/keyframes.html']),
            ('.', ['http/tests/security/view-source-no-refresh.html']),
            ('.', ['http/tests/websocket/tests/websocket-protocol-ignored.html']),
            ('.', ['fast/css/display-none-inline-style-change-crash.html']),
            ('.', ['http/tests/xmlhttprequest/supported-xml-content-types.html']),
            ('.', ['dom/html/level2/html/HTMLAnchorElement03.html']),
            ('.', ['ietestcenter/Javascript/11.1.5_4-4-c-1.html']),
            ('.', ['dom/html/level2/html/HTMLAnchorElement06.html'])])
