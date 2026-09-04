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
from webkitpy.common.unclaimed_shard import UnclaimedShard
from webkitpy.port.server_process import ServerProcess, _log as server_process_logger

_log = logging.getLogger(__name__)


class _NoisyOutputFilter(logging.Filter):
    def filter(self, record):
        msg = record.getMessage()
        if msg.lstrip().startswith('objc['):
            return False
        if 'NSEventConcurrentProcessingEnabled' in msg:
            return False
        return True


_log.addFilter(_NoisyOutputFilter())


def setup_shard(port=None, devices=None, log_limit=None, run_singly=False):
    if devices and getattr(port, 'DEVICE_MANAGER', None):
        port.DEVICE_MANAGER.AVAILABLE_DEVICES = devices.get('available_devices', [])
        port.DEVICE_MANAGER.INITIALIZED_DEVICES = devices.get('initialized_devices', None)
        if devices.get('provisioning'):
            port._provisioning.adopt_provisioning_state(devices['provisioning'])

    return _Worker.setup(port=port, log_limit=log_limit, run_singly=run_singly)


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


class EarlyExitException(Exception):
    def __init__(self, failure_count):
        self.failure_count = failure_count
        super().__init__(f'Exiting early after {failure_count} failures.')


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

    if status in (Runner.STATUS_FAILED, Runner.STATUS_CRASHED, Runner.STATUS_TIMEOUT):
        Runner.instance._failure_count += 1
        if Runner.instance.exit_after_n_failures and Runner.instance._failure_count >= Runner.instance.exit_after_n_failures:
            raise EarlyExitException(Runner.instance._failure_count)


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

    WORKERS_PER_DEVICE = 2

    NAME_FOR_STATUS = [
        'Passed',
        'Failed',
        'Crashed',
        'Timeout',
        'Disabled',
    ]

    instance = None

    def __init__(self, port, printer, log_limit=250, expectations=None):
        self.port = port
        self.printer = printer
        self.tests_run = 0
        self._num_workers = 1
        self.log_limit = log_limit
        self._has_logged_for_test = True  # Suppress an empty line between "Running tests" and the first test's output.
        self.results = {}
        self.expectations = expectations
        self.exit_after_n_failures = None
        self._failure_count = 0
        self._unclaimed_shard_count = 0
        self._abandoned_test_count = 0

    # FIXME API tests should run as an app, we won't need this function <https://bugs.webkit.org/show_bug.cgi?id=175204>
    @staticmethod
    def command_for_port(port, args, worker_number=None):
        if (port.get_option('force')):
            args.append('--force')
        if (port.get_option('remote_layer_tree')):
            args.append('--remote-layer-tree')
        if (port.get_option('site_isolation_enabled_by_default')):
            args.append('--site-isolation-enabled-by-default')
        if (port.get_option('no_remote_layer_tree')):
            args.append('--no-remote-layer-tree')
        if (port.get_option('use_gpu_process')):
            args.append('--use-gpu-process')
        if (port.get_option('no_use_gpu_process')):
            args.append('--no-use-gpu-process')
        if getattr(port, 'DEVICE_MANAGER', None):
            assert port.DEVICE_MANAGER.INITIALIZED_DEVICES
            device = port.target_host(worker_number) if worker_number is not None else port.any_ready_device()
            if device is None:
                device = port.DEVICE_MANAGER.INITIALIZED_DEVICES[0]
            return ['/usr/bin/xcrun', 'simctl', 'spawn', device.udid] + args
        elif 'device' in port.port_name:
            raise RuntimeError(f'Running api tests on {port.port_name} is not supported')
        elif port.host.platform.is_win():
            args[0] = os.path.splitext(args[0])[0] + '.exe'
        return args

    def _handle_unclaimed_shard(self, name, dispatched_count, value, fruitless_attempts, num_workers, redispatch):
        """Sends a shard no device would run back around, or gives up on it.

        Called on the thread that owns the alarm, where devices still coming up get handed over."""
        self.port.advance_provisioning()
        if not isinstance(value, UnclaimedShard) or not value.tests:
            return
        fruitless_attempts = self.port.fruitless_attempts_after(dispatched_count, len(value.tests), fruitless_attempts)
        if self.port.should_abandon_shard(fruitless_attempts, num_workers):
            # Re-dispatching now would spin until the run timed out.
            self._abandoned_test_count += len(value.tests)
            reason = ('No usable devices remain' if not self.port.has_usable_device()
                      else f'No device took {name} after {fruitless_attempts} attempts')
            _log.error('{}; {} tests in {} will not run'.format(reason, len(value.tests), name))
            return
        self._unclaimed_shard_count += 1
        redispatch(value.name, value.tests, fruitless_attempts=fruitless_attempts)

    @staticmethod
    def _shards_longest_first(shards):
        """Biggest shard first, so the longest one is not the last thing left running by itself."""
        return sorted(shards.items(), key=lambda item: (-len(item[1]), item[0]))

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

    @staticmethod
    def _is_disabled_test(test_name):
        # gtest never runs a test whose method component is prefixed with
        # DISABLED_.  test_name is the full binary.suite.method form; strip the
        # binary name, then check the method component.
        components = test_name.split('.')[1:]
        return len(components) > 1 and components[1].startswith('DISABLED_')

    @staticmethod
    def _partition_parallel_safety_tests(tests):
        # In test-parallel-safety mode each supplied test is dispatched as a
        # repeat=True task, so a disabled test loops as an instant no-op and
        # emits "Disabled" without bound until the log limit is hit.  Partition
        # disabled tests out of the repeat set.
        runnable = []
        disabled = []
        for test in tests:
            if Runner._is_disabled_test(test):
                disabled.append(test)
            else:
                runnable.append(test)
        return runnable, disabled

    def run(self, tests, num_workers):
        if not tests:
            return

        if self.port.get_option('fully_parallel') and self.port.get_option('test_parallel_safety'):
            raise RuntimeError(f'Running api tests fully parallel is not compatible with test_parallel_safety')

        self.printer.write_update('Filtering tests by allowlist ...')
        # Split tests by allowlist BEFORE sharding
        allowlisted_tests, non_allowlisted_tests = self.port.filter_api_tests_by_allowlist(tests)

        if non_allowlisted_tests:
            _log.info(f'{len(non_allowlisted_tests)} tests not in allowlist will run in system shard')

        self.printer.write_update('Sharding tests ...')
        # Only shard allowlisted tests for parallel execution
        shards = Runner._shard_tests(allowlisted_tests, self.port.get_option('fully_parallel'))

        original_level = server_process_logger.level
        server_process_logger.setLevel(logging.CRITICAL)

        try:
            if Runner.instance:
                raise RuntimeError('Cannot nest API test runners')
            Runner.instance = self
            mutually_exclusive_groups = list(self.port.sharding_groups(suite='api-tests').keys())
            workers = (num_workers if num_workers and num_workers >= self._num_workers else max(self.port.default_child_processes() or self._num_workers, self._num_workers) if mutually_exclusive_groups else self._num_workers)
            if self.port.devices():
                workers = max(workers, len(self.port.devices()) * Runner.WORKERS_PER_DEVICE)

            devices = None
            if getattr(self.port, 'DEVICE_MANAGER', None):
                devices = dict(
                    available_devices=self.port.DEVICE_MANAGER.AVAILABLE_DEVICES,
                    initialized_devices=self.port.DEVICE_MANAGER.INITIALIZED_DEVICES,
                    provisioning=self.port.provisioning_state(),
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
                supplied_tests, disabled_tests = Runner._partition_parallel_safety_tests(supplied_tests)
                if disabled_tests:
                    _log.warning(f'Test-parallel-safety mode: skipping {len(disabled_tests)} disabled test(s) that cannot be repeat-run: {disabled_tests}')
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

            # Process test-parallel-safety batches sequentially
            for batch_index, test_parallel_safety_batch in enumerate(test_parallel_safety_batches if test_parallel_safety_batches else [[]]):
                if test_parallel_safety_batch:
                    _log.info(f'Running batch {batch_index + 1}/{len(test_parallel_safety_batches)}: {len(shards)} regular shards with {len(test_parallel_safety_batch)} test-parallel-safety repeat tasks')

                non_system_groups = [group for group in mutually_exclusive_groups if group != 'system']
                test_parallel_safety_groups = []
                if test_parallel_safety_batch:
                    # Create unique groups for each test-parallel-safety task so they get separate workers
                    for i, test_name in enumerate(test_parallel_safety_batch):
                        test_parallel_safety_group = f'test-parallel-safety-{i}'
                        test_parallel_safety_groups.append(test_parallel_safety_group)
                        non_system_groups.append(test_parallel_safety_group)
                    batch_test_parallel_safety_count = len(test_parallel_safety_batch)
                    batch_effective_work_count = len(shards) + batch_test_parallel_safety_count
                    batch_workers = min(workers, max(batch_effective_work_count, 1))
                else:
                    batch_workers = self._num_workers

                # Refilled for every pool: a worker holds its device until it exits.
                self.port.prepare_devices_for_workers(workers_per_device=Runner.WORKERS_PER_DEVICE)

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

                    # Run regular shards
                    def dispatch_shard(name, shard_tests, fruitless_attempts=0):
                        pool.do(run_shard, name, *shard_tests, callback=lambda value: handle_shard_result(name, len(shard_tests), value, fruitless_attempts))

                    def handle_shard_result(name, dispatched_count, value, fruitless_attempts):
                        self._handle_unclaimed_shard(name, dispatched_count, value, fruitless_attempts,
                                                     batch_workers, dispatch_shard)

                    for name, shard_tests in Runner._shards_longest_first(shards):
                        if name.startswith('test-parallel-safety.'):
                            continue

                        dispatch_shard(name, shard_tests)

                    pool.wait()

            # Run system shard tests after all parallel tests complete (unless in test-parallel-safety mode)
            if non_allowlisted_tests and not self.port.get_option('test_parallel_safety'):
                # One worker a device: the allowlist does not trust these beside another test on the same one.
                system_shard_workers = max(1, len(self.port.devices()))
                _log.info(f'Running {len(non_allowlisted_tests)} system shard tests across {system_shard_workers} device(s)')
                self.port.prepare_devices_for_workers(workers_per_device=1)
                with TaskPool(
                    workers=system_shard_workers,
                    mutually_exclusive_groups=[],
                    setup=setup_shard, setupkwargs=dict(port=self.port, devices=devices, log_limit=self.log_limit, run_singly=True), teardown=teardown_shard,
                ) as pool:
                    # Group system shard tests by suite for efficiency
                    non_allowlisted_shards = Runner._shard_tests(non_allowlisted_tests, False)

                    def dispatch_system_shard(name, shard_tests, fruitless_attempts=0):
                        pool.do(run_shard, name, *shard_tests, callback=lambda value: handle_system_shard_result(name, len(shard_tests), value, fruitless_attempts))

                    def handle_system_shard_result(name, dispatched_count, value, fruitless_attempts):
                        self._handle_unclaimed_shard(name, dispatched_count, value, fruitless_attempts,
                                                     system_shard_workers, dispatch_system_shard)

                    for name, shard_tests in Runner._shards_longest_first(non_allowlisted_shards):
                        dispatch_system_shard(name, shard_tests)

                    pool.wait()
            elif self.port.get_option('test_parallel_safety'):
                _log.info('Test-parallel-safety mode: skipping all system shard execution')

        finally:
            server_process_logger.setLevel(original_level)
            Runner.instance = None

        if self._abandoned_test_count:
            # Never-run tests are absent from the results, so without this the run reports success having skipped them.
            raise RuntimeError('{} tests could not be run because no device would take them'.format(
                self._abandoned_test_count))

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
    def setup(cls, port=None, log_limit=None, run_singly=False):
        cls.instance = cls(port, log_limit, run_singly)

    @classmethod
    def teardown(cls):
        cls.instance = None

    def __init__(self, port, log_limit, run_singly=False):
        self._port = port
        self.host = port.host
        self.log_limit = log_limit
        self.worker_number = int((TaskPool.Process.name).split('/')[-1]) if TaskPool.Process.name else None
        self._run_singly = run_singly or bool(port.get_option('run_singly'))
        self._device_suspect = True

        # ServerProcess doesn't allow for a timeout of 'None,' this uses a week instead of None.
        self._timeout = int(self._port.get_option('timeout')) if self._port.get_option('timeout') else 60 * 24 * 7

    @classmethod
    def _filter_noisy_output(cls, output):
        result = ''
        for line in output.splitlines():
            if line.lstrip().startswith('objc['):
                continue
            if 'NSEventConcurrentProcessingEnabled' in line:
                continue
            result += line + '\n'
        return result.rstrip()

    def _run_single_test(self, binary_name, test):
        full_test_name = f'{binary_name}.{test}'

        timeout = self._timeout
        if Runner.instance and Runner.instance.expectations:
            exp = Runner.instance.expectations.get_expectation(full_test_name)
            if exp and exp.is_slow():
                custom_timeout = exp.slow_timeout
                if custom_timeout is not None and custom_timeout > 0:
                    timeout = custom_timeout
                else:
                    timeout = self._timeout * 5

        server_process = ServerProcess(
            self._port, binary_name,
            Runner.command_for_port(self._port, [self._port.path_to_api_test(binary_name), '--filter', test], worker_number=self.worker_number),
            env=self._port.environment_for_api_tests())

        status = Runner.STATUS_RUNNING
        if Runner._is_disabled_test(f'{binary_name}.{test}') and not self._port.get_option('force'):
            status = Runner.STATUS_DISABLED

        stdout_buffer = ''
        stderr_buffer = ''
        line_count = 0

        try:
            started = time.time()
            if status != Runner.STATUS_DISABLED:
                server_process.start()

            while status == Runner.STATUS_RUNNING:
                stdout_line, stderr_line = server_process.read_either_stdout_or_stderr_line(started + timeout)
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
                    elif '**DISABLED**' in stdout_line:
                        status = Runner.STATUS_DISABLED
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

        if status in (Runner.STATUS_CRASHED, Runner.STATUS_TIMEOUT):
            self._device_suspect = True

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

        # Only when something already went wrong, since a healthy run should not pay for the check.
        if self._device_suspect and not self._port.target_host_is_usable(self.worker_number):
            _log.error(f'{TaskPool.Process.name} cannot reach its device; returning {name} to be run elsewhere')
            return UnclaimedShard(list(tests), name=name)
        self._device_suspect = False

        # Try to run the shard in a single process.
        while remaining_tests and not self._run_singly:
            starting_length = len(remaining_tests)
            server_process = ServerProcess(
                self._port, binary_name,
                Runner.command_for_port(self._port, [
                    self._port.path_to_api_test(binary_name), '--filter', ':'.join(remaining_tests)
                ], worker_number=self.worker_number), env=self._port.environment_for_api_tests())

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
                        self._device_suspect = True
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
                    if last_test is not None and last_test in remaining_tests:
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
                    # report_result keeps the worse of two statuses, so this crash would outrank a later pass.
                    if last_status == Runner.STATUS_CRASHED and not self._port.target_host_is_usable(self.worker_number, force_update=True):
                        unclaimed = [f'{binary_name}.{remaining}' for remaining in remaining_tests]
                        _log.error(f'{TaskPool.Process.name} lost its device partway through {name}; returning {len(unclaimed)} remaining tests')
                        return UnclaimedShard(unclaimed, name=name)
                    if last_test in remaining_tests:
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
        for index, test in enumerate(remaining_tests):
            if self._device_suspect and not self._port.target_host_is_usable(self.worker_number):
                # Every remaining test would otherwise be recorded as a failure it did not have.
                unclaimed = [f'{binary_name}.{remaining}' for remaining in remaining_tests[index:]]
                _log.error(f'{TaskPool.Process.name} lost its device partway through {name}; returning {len(unclaimed)} remaining tests')
                return UnclaimedShard(unclaimed, name=name)
            self._device_suspect = False
            self._run_single_test(binary_name, test)
