# Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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

import os
import logging
import time

from webkitcorepy import string_utils
from webkitcorepy import TaskPool

from webkitpy.common.iteration_compatibility import iteritems
from webkitpy.port.server_process import ServerProcess, _log as server_process_logger

_log = logging.getLogger(__name__)


def setup_shard(port=None, devices=None, log_limit=None):
    if devices and getattr(port, 'DEVICE_MANAGER', None):
        port.DEVICE_MANAGER.AVAILABLE_DEVICES = devices.get('available_devices', [])
        port.DEVICE_MANAGER.INITIALIZED_DEVICES = devices.get('initialized_devices', None)

    return _Worker.setup(port=port, log_limit=log_limit)


def run_shard(name, *tests):
    return _Worker.instance.run(name, *tests)


def run_test_parallel_safety_single_iteration(test_name):
    """Run a single iteration of test-parallel-safety. TaskPool will handle repeat looping."""
    # Check if worker is ready - repeat tasks can start before setup completes
    if _Worker.instance is None:
        # Worker not ready yet, return early (TaskPool will retry)
        not_ready = f'Worker not ready for {test_name}, skipping iteration'
        _log.debug(not_ready)
        return not_ready

    binary_name = test_name.split('.')[0]
    test_suffix = '.'.join(test_name.split('.')[1:])

    try:
        _Worker.instance._run_single_test(binary_name, test_suffix)
        return f"Test-parallel-safety iteration completed for {test_name}"
    except Exception as e:
        _log.error(f'Error in test-parallel-safety iteration for {test_name}: {e}')
        raise  # Re-raise so TaskPool can handle the error appropriately

def report_result(worker, test, status, output, elapsed=None):
    if elapsed < Runner.ELAPSED_THRESHOLD and status == Runner.STATUS_PASSED and (not output or Runner.instance.port.get_option('quiet')):
        Runner.instance.printer.write_update(f'{worker} {test} {Runner.NAME_FOR_STATUS[status]}')
    else:
        elapsed_log = f' (took {round(elapsed, 1)} seconds)' if elapsed > Runner.ELAPSED_THRESHOLD else ''
        Runner.instance.printer.writeln(f'{worker} {test} {Runner.NAME_FOR_STATUS[status]}{elapsed_log}')
    if test in Runner.instance.results:
        existing_status = Runner.instance.results[test][0]
        if status > existing_status or (status == existing_status and status != Runner.STATUS_PASSED):
            Runner.instance.results[test] = status, output, elapsed
    else:
        Runner.instance.results[test] = status, output, elapsed


def teardown_shard():
    return _Worker.teardown()


class Runner(object):
    STATUS_PASSED = 0
    STATUS_FAILED = 1
    STATUS_CRASHED = 2
    STATUS_TIMEOUT = 3
    STATUS_DISABLED = 4
    STATUS_RUNNING = 5

    ELAPSED_THRESHOLD = 3

    NAME_FOR_STATUS = [
        'Passed',
        'Failed',
        'Crashed',
        'Timeout',
        'Disabled',
    ]

    instance = None

    def __init__(self, port, printer, log_limit=250):
        self.port = port
        self.printer = printer
        self.tests_run = 0
        self._num_workers = 1
        self.log_limit = log_limit
        self._has_logged_for_test = True  # Suppress an empty line between "Running tests" and the first test's output.
        self.results = {}

    # FIXME API tests should run as an app, we won't need this function <https://bugs.webkit.org/show_bug.cgi?id=175204>
    @staticmethod
    def command_for_port(port, args):
        if (port.get_option('force')):
            args.append('--gtest_also_run_disabled_tests=1')
        if (port.get_option('remote_layer_tree')):
            args.append('--remote-layer-tree')
        if (port.get_option('site_isolation')):
            args.append('--site-isolation')
        if (port.get_option('no_remote_layer_tree')):
            args.append('--no-remote-layer-tree')
        if (port.get_option('use_gpu_process')):
            args.append('--use-gpu-process')
        if (port.get_option('no_use_gpu_process')):
            args.append('--no-use-gpu-process')
        if getattr(port, 'DEVICE_MANAGER', None):
            assert port.DEVICE_MANAGER.INITIALIZED_DEVICES
            return ['/usr/bin/xcrun', 'simctl', 'spawn', port.DEVICE_MANAGER.INITIALIZED_DEVICES[0].udid] + args
        elif 'device' in port.port_name:
            raise RuntimeError(f'Running api tests on {port.port_name} is not supported')
        elif port.host.platform.is_win():
            args[0] = os.path.splitext(args[0])[0] + '.exe'
        return args

    @staticmethod
    def _shard_tests(tests, fully_parallel):
        shards = {}
        for test in tests:
            shard_prefix = '.'.join(test.split('.')[:-1])
            if shard_prefix not in shards:
                shards[shard_prefix] = []
            shards[shard_prefix].append(test)
        if fully_parallel:
            shards = {}
            for i, test in enumerate(tests):
                shards[f"{test}.{i}"] = [test]
        return shards

    def run(self, tests, num_workers):
        if not tests:
            return

        if self.port.get_option('fully_parallel') and self.port.get_option('test_parallel_safety'):
            raise RuntimeError(f'Running api tests fully parallel is not compatible with test_parallel_safety')

        self.printer.write_update('Sharding tests ...')
        shards = Runner._shard_tests(tests, self.port.get_option('fully_parallel'))

        original_level = server_process_logger.level
        server_process_logger.setLevel(logging.CRITICAL)

        try:
            if Runner.instance:
                raise RuntimeError('Cannot nest API test runners')
            Runner.instance = self
            mutually_exclusive_groups = list(self.port.sharding_groups(suite='api-tests').keys())
            workers = (num_workers if num_workers and num_workers >= self._num_workers else max(self.port.default_child_processes() or self._num_workers, self._num_workers) if mutually_exclusive_groups else self._num_workers)

            devices = None
            if getattr(self.port, 'DEVICE_MANAGER', None):
                devices = dict(
                    available_devices=self.port.DEVICE_MANAGER.AVAILABLE_DEVICES,
                    initialized_devices=self.port.DEVICE_MANAGER.INITIALIZED_DEVICES,
                )

            supplied_tests_raw = self.port.get_option('test_parallel_safety')
            # Reserve 1/3 of workers for repeat tasks
            max_repeat_workers = max(1, workers // 3)
            supplied_tests = []
            test_parallel_safety_batches = []
            # Calculate batching for test-parallel-safety tasks to prevent overwhelming workers
            if supplied_tests_raw:
                for test_arg in supplied_tests_raw:
                    supplied_tests.extend(test_arg.split())
                _log.info(f'Test-parallel-safety mode: creating repeat loop tasks for tests: {supplied_tests}')

                if len(supplied_tests) <= max_repeat_workers:
                    # All tasks fit in one batch
                    test_parallel_safety_batches = [supplied_tests]
                else:
                    # Split into batches of max_repeat_workers
                    for i in range(0, len(supplied_tests), max_repeat_workers):
                        batch = supplied_tests[i:i + max_repeat_workers]
                        test_parallel_safety_batches.append(batch)
                    _log.info(f'Test-parallel-safety batching: {len(supplied_tests)} tasks split into {len(test_parallel_safety_batches)} batches of max {max_repeat_workers} tasks each')

            # For minimum worker calculation, use current batch size (first batch or all if unbatched)
            self._num_workers = min(workers, max(len(shards) + (len(test_parallel_safety_batches[0]) if test_parallel_safety_batches else 0), 1))

            system_shards = {}
            non_system_shards = {}

            for name, tests in iteritems(shards):
                group = self.port.group_for_shard(type('Shard', (), {'name': name})(), suite='api-tests')
                if group == 'system' and not self.port.get_option('fully_parallel'):
                    system_shards[name] = tests
                else:
                    non_system_shards[name] = tests

            # Process test-parallel-safety batches sequentially
            for batch_index, test_parallel_safety_batch in enumerate(test_parallel_safety_batches if test_parallel_safety_batches else [[]]):
                if test_parallel_safety_batch:
                    _log.info(f'Running batch {batch_index + 1}/{len(test_parallel_safety_batches)}: {len(non_system_shards)} regular shards with {len(test_parallel_safety_batch)} test-parallel-safety repeat tasks')

                non_system_groups = [group for group in mutually_exclusive_groups if group != 'system']
                test_parallel_safety_groups = []
                if test_parallel_safety_batch:
                    # Create unique groups for each test-parallel-safety task so they get separate workers
                    for i, test_name in enumerate(test_parallel_safety_batch):
                        test_parallel_safety_group = f'test-parallel-safety-{i}'
                        test_parallel_safety_groups.append(test_parallel_safety_group)
                        non_system_groups.append(test_parallel_safety_group)
                    batch_test_parallel_safety_count = len(test_parallel_safety_batch)
                    batch_effective_work_count = len(non_system_shards) + batch_test_parallel_safety_count
                    batch_workers = min(workers, max(batch_effective_work_count, 1))
                else:
                    batch_workers = self._num_workers

                with TaskPool(
                    workers=batch_workers,
                    mutually_exclusive_groups=non_system_groups,
                    setup=setup_shard, setupkwargs=dict(port=self.port, devices=devices, log_limit=self.log_limit), teardown=teardown_shard,
                ) as pool:

                    if test_parallel_safety_batch:
                        for i, test_name in enumerate(test_parallel_safety_batch):
                            test_parallel_safety_group = test_parallel_safety_groups[i]
                            _log.info(f'Dispatching repeat test-parallel-safety task for {test_name} to group {test_parallel_safety_group} (batch {batch_index + 1}/{len(test_parallel_safety_batches)})')
                            pool.do(run_test_parallel_safety_single_iteration, test_name, repeat=True, group=test_parallel_safety_group)

                    # Run regular shards with each batch - this is the whole point of test-parallel-safety testing
                    for name, tests in iteritems(non_system_shards):
                        if name.startswith('test-parallel-safety.'):
                            continue

                        group = self.port.group_for_shard(type('Shard', (), {'name': name})(), suite='api-tests')
                        if test_parallel_safety_batches and group == 'system':
                            continue

                        if group and group != 'system' and not self.port.get_option('fully_parallel'):
                            pool.do(run_shard, name, *tests, group=group)
                        else:
                            pool.do(run_shard, name, *tests)

                    pool.wait()

            # Run system tests after all non-system tests complete (unless in test-parallel-safety mode)
            if system_shards and not self.port.get_option('test_parallel_safety'):
                with TaskPool(
                    workers=1,  # System tests run with single worker to avoid conflicts
                    mutually_exclusive_groups=[],
                    setup=setup_shard, setupkwargs=dict(port=self.port, devices=devices, log_limit=self.log_limit), teardown=teardown_shard,
                ) as pool:
                    for name, tests in iteritems(system_shards):
                        pool.do(run_shard, name, *tests)

                    pool.wait()
            elif self.port.get_option('test_parallel_safety'):
                _log.info('Test-parallel-safety mode: skipping all system shard execution')

        finally:
            server_process_logger.setLevel(original_level)
            Runner.instance = None

    def result_map_by_status(self, status=None):
        map = {}
        for test_name, result in iteritems(self.results):
            if result[0] == status:
                map[test_name] = result[1]
        return map


class _Worker(object):
    instance = None
    EXCEEDED_LOG_LINE_MESSAGE = 'EXCEEDED LOG LINE THRESHOLD OF {}\n'

    @classmethod
    def setup(cls, port=None, log_limit=None):
        cls.instance = cls(port, log_limit)

    @classmethod
    def teardown(cls):
        cls.instance = None

    def __init__(self, port, log_limit):
        self._port = port
        self.host = port.host
        self.log_limit = log_limit

        # ServerProcess doesn't allow for a timeout of 'None,' this uses a week instead of None.
        self._timeout = int(self._port.get_option('timeout')) if self._port.get_option('timeout') else 60 * 24 * 7

    @classmethod
    def _filter_noisy_output(cls, output):
        result = ''
        for line in output.splitlines():
            if line.lstrip().startswith('objc['):
                continue
            result += line + '\n'
        return result.rstrip()

    def _run_single_test(self, binary_name, test):
        server_process = ServerProcess(
            self._port, binary_name,
            Runner.command_for_port(self._port, [self._port.path_to_api_test(binary_name), f'--gtest_filter={test}']),
            env=self._port.environment_for_api_tests())

        status = Runner.STATUS_RUNNING
        if test.split('.')[1].startswith('DISABLED_') and not self._port.get_option('force'):
            status = Runner.STATUS_DISABLED

        stdout_buffer = ''
        stderr_buffer = ''
        line_count = 0

        try:
            started = time.time()
            if status != Runner.STATUS_DISABLED:
                server_process.start()

            while status == Runner.STATUS_RUNNING:
                stdout_line, stderr_line = server_process.read_either_stdout_or_stderr_line(started + self._timeout)
                if not stderr_line and not stdout_line:
                    break
                if stdout_line:
                    line_count += 1
                if stderr_line:
                    line_count += 1
                if line_count > self.log_limit:
                    stderr_line = self.EXCEEDED_LOG_LINE_MESSAGE.format(self.log_limit)
                    stderr_buffer += stderr_line
                    _log.error(stderr_line[:-1])
                    server_process.stop()
                    status = Runner.STATUS_FAILED
                    break

                if stderr_line:
                    stderr_line = string_utils.decode(stderr_line, target_type=str)
                    stderr_buffer += stderr_line
                    _log.error(stderr_line[:-1])
                if stdout_line:
                    stdout_line = string_utils.decode(stdout_line, target_type=str)
                    if '**PASS**' in stdout_line:
                        status = Runner.STATUS_PASSED
                    elif '**FAIL**' in stdout_line:
                        status = Runner.STATUS_FAILED
                    else:
                        stdout_buffer += stdout_line
                        _log.error(stdout_line[:-1])

            if status == Runner.STATUS_DISABLED:
                pass
            elif server_process.timed_out:
                status = Runner.STATUS_TIMEOUT
            elif server_process.has_crashed():
                status = Runner.STATUS_CRASHED
            elif status == Runner.STATUS_RUNNING:
                status = Runner.STATUS_FAILED

        finally:
            output_buffer = stderr_buffer + stdout_buffer
            remaining_stderr = string_utils.decode(server_process.pop_all_buffered_stderr(), target_type=str)
            remaining_stdout = string_utils.decode(server_process.pop_all_buffered_stdout(), target_type=str)
            for line in (remaining_stdout + remaining_stderr).splitlines(False):
                line_count += 1
                if line_count > self.log_limit:
                    status = Runner.STATUS_FAILED
                    line = self.EXCEEDED_LOG_LINE_MESSAGE.format(self.log_limit)

                _log.error(line)
                output_buffer += line + '\n'

                if line_count > self.log_limit:
                    break

            server_process.stop()

        TaskPool.Process.queue.send(TaskPool.Task(
            report_result, None, TaskPool.Process.name,
            f'{binary_name}.{test}',
            status,
            self._filter_noisy_output(output_buffer),
            elapsed=time.time() - started,
        ))

    def run(self, name, *tests):
        binary_name = name.split('.')[0]
        remaining_tests = ['.'.join(test.split('.')[1:]) for test in tests]

        # Try to run the shard in a single process.
        while remaining_tests and not self._port.get_option('run_singly'):
            starting_length = len(remaining_tests)
            server_process = ServerProcess(
                self._port, binary_name,
                Runner.command_for_port(self._port, [
                    self._port.path_to_api_test(binary_name), f'--gtest_filter={":".join(remaining_tests)}'
                ]), env=self._port.environment_for_api_tests())

            try:
                started = time.time()
                last_test = None
                last_status = None
                buffer = ''
                line_count = 0

                server_process.start()
                while remaining_tests:
                    stdout = string_utils.decode(server_process.read_stdout_line(started + self._timeout), target_type=str)

                    # If we've triggered a timeout, we don't know which test caused it. Break out and run singly.
                    if stdout is None and server_process.timed_out:
                        break

                    if stdout is None and server_process.has_crashed():
                        # It's possible we crashed before printing anything.
                        if last_status == Runner.STATUS_PASSED:
                            last_test = None
                        else:
                            last_status = Runner.STATUS_CRASHED
                        break

                    assert stdout is not None
                    stdout_split = stdout.rstrip().split(' ')

                    line_count += len(stdout_split)
                    if line_count > self.log_limit:
                        break

                    if len(stdout_split) != 2 or not (stdout_split[0].startswith('**') and stdout_split[0].endswith('**')):
                        buffer += stdout
                        continue
                    if last_test is not None:
                        remaining_tests.remove(last_test)

                        for line in buffer.splitlines(False):
                            _log.error(line)
                        line_count = 0
                        TaskPool.Process.queue.send(TaskPool.Task(
                            report_result, None, TaskPool.Process.name,
                            f'{binary_name}.{last_test}',
                            last_status, buffer,
                            elapsed=time.time() - started,
                        ))
                        started = time.time()
                        buffer = ''

                    if '**PASS**' == stdout_split[0]:
                        last_status = Runner.STATUS_PASSED
                    else:
                        last_status = Runner.STATUS_FAILED
                    last_test = stdout_split[1]

                # We assume that stderr is only relevant if there is a crash (meaning we triggered an assert)
                if last_test:
                    remaining_tests.remove(last_test)
                    stdout_buffer = string_utils.decode(server_process.pop_all_buffered_stdout(), target_type=str)
                    stderr_buffer = string_utils.decode(server_process.pop_all_buffered_stderr(), target_type=str) if last_status == Runner.STATUS_CRASHED else ''
                    for line in (stdout_buffer + stderr_buffer).splitlines():
                        line_count += 1
                        if line_count > self.log_limit:
                            break
                        buffer += line
                        _log.error(line[:-1])

                    if line_count > self.log_limit:
                        last_status = Runner.STATUS_FAILED
                        line = self.EXCEEDED_LOG_LINE_MESSAGE.format(self.log_limit)
                        buffer += line
                        _log.error(line[:-1])

                    TaskPool.Process.queue.send(TaskPool.Task(
                        report_result, None, TaskPool.Process.name,
                        f'{binary_name}.{last_test}',
                        last_status,
                        self._filter_noisy_output(buffer),
                        elapsed=time.time() - started,
                    ))

                if server_process.timed_out:
                    break

                # If we weren't able to determine the results for any tests, we need to run what remains singly.
                if starting_length == len(remaining_tests):
                    break
            finally:
                server_process.stop()

        # Now, just try and run the rest of the tests singly.
        for test in remaining_tests:
            self._run_single_test(binary_name, test)
