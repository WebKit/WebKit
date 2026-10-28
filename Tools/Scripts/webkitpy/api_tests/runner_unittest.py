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
from unittest.mock import MagicMock, patch

from webkitpy.api_tests.runner import EarlyExitException, Runner, report_result, run_background_load, run_parallel_safety_iterations, run_shard
from webkitpy.common.program_status import ProgramStatus
from webkitpy.common.program_status_mock import TTYStringIO, encoded


class FakeDevice(object):
    def __init__(self, udid):
        self.udid = udid


def _port_with_devices(devices):
    port = MagicMock(port_name='ios-simulator')
    port.get_option.return_value = None
    port.DEVICE_MANAGER.INITIALIZED_DEVICES = devices
    port.target_host.side_effect = lambda worker_number: devices[worker_number]
    return port


class RunnerTest(unittest.TestCase):
    def test_is_disabled_test_detects_disabled_method(self):
        self.assertTrue(Runner._is_disabled_test(
            "TestWebKitAPI.SiteIsolation.DISABLED_PostMessageWithMessagePorts"))

    def test_is_disabled_test_allows_enabled_test(self):
        self.assertFalse(Runner._is_disabled_test(
            "TestWebKitAPI.SiteIsolation.LoadingCallbacksAndPostMessage"))

    def test_chunk_shards_splits_large_shards(self):
        shards = {
            'TestWebKitAPI.Big': [f'TestWebKitAPI.Big.Test{index}' for index in range(5)],
            'TestWebKitAPI.Small': ['TestWebKitAPI.Small.Only'],
        }
        self.assertEqual(Runner._chunk_shards(shards, 2), [
            ('TestWebKitAPI.Big', ['TestWebKitAPI.Big.Test0', 'TestWebKitAPI.Big.Test1']),
            ('TestWebKitAPI.Big', ['TestWebKitAPI.Big.Test2', 'TestWebKitAPI.Big.Test3']),
            ('TestWebKitAPI.Big', ['TestWebKitAPI.Big.Test4']),
            ('TestWebKitAPI.Small', ['TestWebKitAPI.Small.Only']),
        ])

    def test_background_chunks_cover_every_test_in_a_shuffled_order(self):
        background = [f'TestWebKitAPI.Suite{suite}.Test{index}' for suite in range(10) for index in range(3)]
        chunks = Runner._parallel_safety_background_chunks(background, ['TestWebKitAPI.New.Test'])
        self.assertEqual(sorted(test for _, tests in chunks for test in tests), sorted(background))
        self.assertNotEqual([name for name, _ in chunks], sorted(name for name, _ in chunks))

    def test_background_chunk_order_depends_only_on_the_checked_tests(self):
        background = [f'TestWebKitAPI.Suite{suite}.Test' for suite in range(10)]
        first = Runner._parallel_safety_background_chunks(background, ['TestWebKitAPI.New.A'])
        self.assertEqual(Runner._parallel_safety_background_chunks(background, ['TestWebKitAPI.New.A']), first)
        self.assertNotEqual(Runner._parallel_safety_background_chunks(background, ['TestWebKitAPI.New.B']), first)


class RunnerRunTest(unittest.TestCase):
    ALLOWLISTED = [
        'TestWebKitAPI.SiteIsolation.Target',
        'TestWebKitAPI.SiteIsolation.Other',
        'TestWebKitAPI.WebKit.A',
        'TestWebKitAPI.WebKit.B',
    ]
    NOT_ALLOWLISTED = ['TestWebKitAPI.System.Target']

    def setUp(self):
        self._saved_instance = Runner.instance
        Runner.instance = None

    def tearDown(self):
        Runner.instance = self._saved_instance

    def _run(self, parallel_safety_tests, num_workers=6, iterations=3):
        options = dict(test_parallel_safety=parallel_safety_tests, parallel_safety_iterations=iterations, fully_parallel=False)
        port = MagicMock()
        port.get_option.side_effect = lambda name: options.get(name)
        port.filter_api_tests_by_allowlist.return_value = (self.ALLOWLISTED, self.NOT_ALLOWLISTED)
        port.sharding_groups.return_value = {}
        with patch('webkitpy.api_tests.runner.TaskPool') as task_pool:
            Runner(port, MagicMock()).run(self.ALLOWLISTED + self.NOT_ALLOWLISTED, num_workers)
        return task_pool

    @staticmethod
    def _calls(task_pool, function):
        return [call for call in task_pool.return_value.__enter__.return_value.do.call_args_list if call.args[0] is function]

    def test_parallel_safety_runs_each_checked_test_in_one_task_and_leaves_their_suites_out_of_the_background(self):
        task_pool = self._run(['TestWebKitAPI.SiteIsolation.Target', 'TestWebKitAPI.System.Target'])

        self.assertEqual(task_pool.call_count, 1)
        checked_calls = self._calls(task_pool, run_parallel_safety_iterations)
        self.assertEqual([call.args[1:] for call in checked_calls], [('TestWebKitAPI.SiteIsolation.Target', 3), ('TestWebKitAPI.System.Target', 3)])
        self.assertFalse(any(call.kwargs.get('repeat') for call in checked_calls))

        background_calls = self._calls(task_pool, run_background_load)
        self.assertTrue(all(call.kwargs['repeat'] for call in background_calls))
        self.assertEqual(sorted(call.kwargs['group'] for call in background_calls), sorted(task_pool.call_args.kwargs['mutually_exclusive_groups']))
        background_tests = sorted(test for call in background_calls for _, tests in call.args[1] for test in tests)
        self.assertEqual(background_tests, ['TestWebKitAPI.WebKit.A', 'TestWebKitAPI.WebKit.B'])
        self.assertEqual(task_pool.call_args.kwargs['workers'], 2 + len(background_calls))

    def test_parallel_safety_checks_a_test_outside_the_allowlist(self):
        task_pool = self._run(['TestWebKitAPI.System.Target'], iterations=1)

        self.assertEqual([call.args[1:] for call in self._calls(task_pool, run_parallel_safety_iterations)], [('TestWebKitAPI.System.Target', 1)])
        background_tests = sorted(test for call in self._calls(task_pool, run_background_load) for _, tests in call.args[1] for test in tests)
        self.assertEqual(background_tests, sorted(self.ALLOWLISTED))
        self.assertEqual(self._calls(task_pool, run_shard), [])

    def test_parallel_safety_skips_unavailable_tests(self):
        task_pool = self._run(['TestWebKitAPI.SiteIsolation.Unavailable'])
        self.assertEqual(task_pool.call_count, 0)

    def test_parallel_safety_never_uses_more_workers_than_requested(self):
        task_pool = self._run(['TestWebKitAPI.SiteIsolation.Target'], num_workers=1)

        self.assertEqual(task_pool.call_args.kwargs['workers'], 1)
        self.assertEqual(task_pool.call_args.kwargs['mutually_exclusive_groups'], [])
        self.assertEqual([call.args[1:] for call in self._calls(task_pool, run_parallel_safety_iterations)], [('TestWebKitAPI.SiteIsolation.Target', 3)])
        self.assertEqual(self._calls(task_pool, run_background_load), [])

    def test_parallel_safety_with_two_workers_runs_background_on_one(self):
        task_pool = self._run(['TestWebKitAPI.SiteIsolation.Target'], num_workers=2)

        self.assertEqual(task_pool.call_args.kwargs['workers'], 2)
        self.assertEqual(len(self._calls(task_pool, run_background_load)), 1)

    def test_without_parallel_safety_runs_shards_then_the_system_shard(self):
        task_pool = self._run(None)

        self.assertEqual(task_pool.call_count, 2)
        self.assertEqual([call.kwargs['workers'] for call in task_pool.call_args_list], [2, 1])
        shard_names = [call.args[1] for call in self._calls(task_pool, run_shard)]
        self.assertEqual(shard_names, ['TestWebKitAPI.SiteIsolation', 'TestWebKitAPI.WebKit', 'TestWebKitAPI.System'])
        self.assertEqual(self._calls(task_pool, run_background_load), [])


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


class ProgramStatusTest(unittest.TestCase):
    def _runner(self, test_parallel_safety=False):
        port = MagicMock()
        port.get_option.side_effect = lambda name: test_parallel_safety if name == 'test_parallel_safety' else None
        stream = TTYStringIO()
        return Runner(port, MagicMock(), status=ProgramStatus(stream, 'run-api-tests')), stream

    def test_reports_progress_as_tests_finish(self):
        runner, stream = self._runner()
        runner.begin_tests(4)
        runner.test_finished()
        runner.test_finished()
        self.assertEqual(stream.program_status_reports(), [
            'state=working:progress=0:app=run-api-tests',
            'state=working:progress=25:app=run-api-tests',
            'state=working:progress=50:app=run-api-tests',
        ])

    def test_reports_failures_so_far(self):
        runner, stream = self._runner()
        saved_instance, Runner.instance = Runner.instance, runner
        self.addCleanup(setattr, Runner, 'instance', saved_instance)
        runner.begin_tests(4)
        report_result('worker', 'TestWTF.Suite.Test', Runner.STATUS_FAILED, output='err', elapsed=0.1)
        self.assertEqual(stream.program_status_reports(), [
            'state=working:progress=0:app=run-api-tests',
            'state=working:progress=25:app=run-api-tests:msg=' + encoded('1 failure'),
        ])

    def test_restarts_progress_for_each_iteration(self):
        runner, stream = self._runner()
        runner.begin_tests(1)
        runner.test_finished()
        runner.begin_tests(1)
        self.assertEqual(stream.program_status_reports()[-2:], [
            'state=working:progress=100:app=run-api-tests',
            'state=working:progress=0:app=run-api-tests',
        ])

    def test_progress_is_indeterminate_in_test_parallel_safety_mode(self):
        runner, stream = self._runner(test_parallel_safety=True)
        runner.begin_tests(2)
        runner.test_finished()
        self.assertEqual(stream.program_status_reports(), ['state=working:app=run-api-tests'])


class CommandForPortTest(unittest.TestCase):

    def test_each_worker_spawns_on_its_own_device(self):
        port = _port_with_devices([FakeDevice('udid-0'), FakeDevice('udid-1')])
        self.assertEqual(
            Runner.command_for_port(port, ['TestWTF'], worker_number=1),
            ['/usr/bin/xcrun', 'simctl', 'spawn', 'udid-1', 'TestWTF'])

    def test_work_outside_a_worker_uses_the_first_device(self):
        port = _port_with_devices([FakeDevice('udid-0'), FakeDevice('udid-1')])
        self.assertEqual(
            Runner.command_for_port(port, ['TestWTF']),
            ['/usr/bin/xcrun', 'simctl', 'spawn', 'udid-0', 'TestWTF'])


if __name__ == '__main__':
    unittest.main()
