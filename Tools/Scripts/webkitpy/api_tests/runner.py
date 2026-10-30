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
import random
import time

from webkitcorepy import string_utils
from webkitcorepy import TaskPool

from webkitpy.common.iteration_compatibility import iteritems
from webkitpy.common.program_status import ProgramStatus
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


def setup_shard(port=None, devices=None, log_limit=None):
    if devices and getattr(port, 'DEVICE_MANAGER', None):
        port.DEVICE_MANAGER.AVAILABLE_DEVICES = devices.get('available_devices', [])
        port.DEVICE_MANAGER.INITIALIZED_DEVICES = devices.get('initialized_devices', None)

    return _Worker.setup(port=port, log_limit=log_limit)


def run_shard(name, *tests):
    return _Worker.instance.run(name, *tests)


def run_parallel_safety_iterations(test_name, iterations):
    binary_name, _, test = test_name.partition('.')
    for _ in range(iterations):
        _Worker.instance._run_single_test(binary_name, test)


def run_background_load(chunks):
    _Worker.instance.run_next_background_chunk(chunks)


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

    failed = status in (Runner.STATUS_FAILED, Runner.STATUS_CRASHED, Runner.STATUS_TIMEOUT)
    if failed:
        Runner.instance._failure_count += 1
    Runner.instance.test_finished()
    if failed and Runner.instance.exit_after_n_failures and Runner.instance._failure_count >= Runner.instance.exit_after_n_failures:
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

    # Background load only stops between chunks, so this bounds how long it outlives the tests being checked.
    PARALLEL_SAFETY_BACKGROUND_CHUNK_SIZE = 20

    NAME_FOR_STATUS = [
        'Passed',
        'Failed',
        'Crashed',
        'Timeout',
        'Disabled',
    ]

    instance = None

    def __init__(self, port, printer, log_limit=250, expectations=None, status=None):
        self.port = port
        self.printer = printer
        self.tests_run = 0
        self._status = status or ProgramStatus.disabled()
        self._tests_total = 0
        self._tests_finished = 0
        self._num_workers = 1
        self.log_limit = log_limit
        self._has_logged_for_test = True  # Suppress an empty line between "Running tests" and the first test's output.
        self.results = {}
        self.expectations = expectations
        self.exit_after_n_failures = None
        self._failure_count = 0

    # FIXME API tests should run as an app, we won't need this function <https://bugs.webkit.org/show_bug.cgi?id=175204>
    @staticmethod
    def command_for_port(port, args, worker_number=None):
        if (port.get_option('force')):
            args.append('--force')
        if (port.get_option('site_isolation_enabled_by_default')):
            args.append('--site-isolation-enabled-by-default')
        if (port.get_option('use_gpu_process')):
            args.append('--use-gpu-process')
        if (port.get_option('no_use_gpu_process')):
            args.append('--no-use-gpu-process')
        if getattr(port, 'DEVICE_MANAGER', None):
            assert port.DEVICE_MANAGER.INITIALIZED_DEVICES
            device = port.target_host(worker_number) if worker_number is not None else port.DEVICE_MANAGER.INITIALIZED_DEVICES[0]
            return ['/usr/bin/xcrun', 'simctl', 'spawn', device.udid] + args
        elif 'device' in port.port_name:
            raise RuntimeError(f'Running api tests on {port.port_name} is not supported')
        elif port.host.platform.is_win():
            args[0] = os.path.splitext(args[0])[0] + '.exe'
        return args

    @staticmethod
    def _suite_name(test):
        return '.'.join(test.split('.')[:-1])

    @staticmethod
    def _shard_tests(tests, fully_parallel):
        shards = {}
        for test in tests:
            suite = Runner._suite_name(test)
            if suite not in shards:
                shards[suite] = []
            shards[suite].append(test)
        if fully_parallel:
            shards = {}
            for i, test in enumerate(tests):
                shards[f"{test}.{i}"] = [test]
        return shards

    @staticmethod
    def _chunk_shards(shards, chunk_size):
        return [(name, shard_tests[index:index + chunk_size]) for name, shard_tests in iteritems(shards) for index in range(0, len(shard_tests), chunk_size)]

    @staticmethod
    def _parallel_safety_background_chunks(background_tests, checked_tests):
        chunks = Runner._chunk_shards(Runner._shard_tests(background_tests, False), Runner.PARALLEL_SAFETY_BACKGROUND_CHUNK_SIZE)
        # Tests arrive sorted, so without shuffling every run would only reach the first suites before the checked tests finish.
        random.Random(' '.join(checked_tests)).shuffle(chunks)
        return chunks

    @staticmethod
    def _is_disabled_test(test_name):
        # gtest never runs a test whose method component is prefixed with
        # DISABLED_.  test_name is the full binary.suite.method form; strip the
        # binary name, then check the method component.
        components = test_name.split('.')[1:]
        return len(components) > 1 and components[1].startswith('DISABLED_')

    def begin_tests(self, total):
        self._tests_total = total
        self._tests_finished = 0
        self._report_progress()

    def test_finished(self):
        self._tests_finished += 1
        self._report_progress()

    def _report_progress(self):
        progress = None
        if self._tests_total and not self.port.get_option('test_parallel_safety'):
            progress = min(self._tests_finished * 100 // self._tests_total, 100)
        msg = string_utils.pluralize(self._failure_count, 'failure') if self._failure_count else None
        self._status.report('working', progress=progress, msg=msg)

    def run(self, tests, num_workers):
        if not tests:
            return

        if self.port.get_option('fully_parallel') and self.port.get_option('test_parallel_safety'):
            raise RuntimeError(f'Running api tests fully parallel is not compatible with test_parallel_safety')

        self.begin_tests(len(tests))

        self.printer.write_update('Filtering tests by allowlist ...')
        # Split tests by allowlist BEFORE sharding
        allowlisted_tests, non_allowlisted_tests = self.port.filter_api_tests_by_allowlist(tests)

        if non_allowlisted_tests and not self.port.get_option('test_parallel_safety'):
            _log.info(f'{len(non_allowlisted_tests)} tests not in allowlist will run in system shard')

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
            setupkwargs = dict(port=self.port, devices=devices, log_limit=self.log_limit)

            if self.port.get_option('test_parallel_safety'):
                self._run_parallel_safety(tests, allowlisted_tests, workers, setupkwargs)
                return

            self.printer.write_update('Sharding tests ...')
            # Only shard allowlisted tests for parallel execution
            shards = Runner._shard_tests(allowlisted_tests, self.port.get_option('fully_parallel'))
            self._num_workers = min(workers, max(len(shards), 1))

            with TaskPool(
                workers=self._num_workers,
                mutually_exclusive_groups=[group for group in mutually_exclusive_groups if group != 'system'],
                setup=setup_shard, setupkwargs=setupkwargs, teardown=teardown_shard,
            ) as pool:
                for name, shard_tests in iteritems(shards):
                    pool.do(run_shard, name, *shard_tests)

                pool.wait()

            # Run system shard tests after all parallel tests complete
            if non_allowlisted_tests:
                _log.info(f'Running {len(non_allowlisted_tests)} system shard tests sequentially')
                with TaskPool(
                    workers=1,  # System shard tests run with single worker to avoid conflicts
                    mutually_exclusive_groups=[],
                    setup=setup_shard, setupkwargs=setupkwargs, teardown=teardown_shard,
                ) as pool:
                    # Group system shard tests by suite for efficiency
                    non_allowlisted_shards = Runner._shard_tests(non_allowlisted_tests, False)
                    for name, shard_tests in iteritems(non_allowlisted_shards):
                        pool.do(run_shard, name, *shard_tests)

                    pool.wait()

        finally:
            server_process_logger.setLevel(original_level)
            Runner.instance = None

    def _run_parallel_safety(self, tests, allowlisted_tests, workers, setupkwargs):
        supplied_tests = []
        for test_arg in self.port.get_option('test_parallel_safety'):
            supplied_tests.extend(test_arg.split())

        available_tests = set(tests)
        checked_tests = [test for test in supplied_tests if test in available_tests]
        unavailable_tests = [test for test in supplied_tests if test not in available_tests]
        if unavailable_tests:
            _log.info(f'Test-parallel-safety mode: skipping {len(unavailable_tests)} test(s) that are skipped or unavailable: {unavailable_tests}')
        if not checked_tests:
            _log.info('Test-parallel-safety mode: no tests to check')
            return

        checked_suites = {Runner._suite_name(test) for test in checked_tests}
        background_chunks = Runner._parallel_safety_background_chunks([test for test in allowlisted_tests if Runner._suite_name(test) not in checked_suites], checked_tests)
        iterations = self.port.get_option('parallel_safety_iterations') or 1
        checked_workers = min(len(checked_tests), max(1, workers // 3))
        background_workers = min(len(background_chunks), workers - checked_workers)
        if background_chunks and not background_workers:
            _log.warning('Test-parallel-safety mode: only one worker, so the tests are checked without background load')
        background_groups = [f'parallel-safety-background-{index}' for index in range(background_workers)]

        _log.info(f'Test-parallel-safety mode: running {len(checked_tests)} test(s) {iterations} time(s) each on {checked_workers} worker(s), '
                  f'with {len(background_chunks)} background chunk(s) on {background_workers} worker(s): {checked_tests}')

        self._num_workers = checked_workers + background_workers
        with TaskPool(
            workers=self._num_workers,
            mutually_exclusive_groups=background_groups,
            setup=setup_shard, setupkwargs=setupkwargs, teardown=teardown_shard,
        ) as pool:
            for index, group in enumerate(background_groups):
                pool.do(run_background_load, background_chunks[index::background_workers], repeat=True, group=group)
            # One task per test, so a test never runs alongside another copy of itself.
            for test in checked_tests:
                pool.do(run_parallel_safety_iterations, test, iterations)
            pool.wait()

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
        self.worker_number = int((TaskPool.Process.name).split('/')[-1]) if TaskPool.Process.name else None

        # ServerProcess doesn't allow for a timeout of 'None,' this uses a week instead of None.
        self._timeout = int(self._port.get_option('timeout')) if self._port.get_option('timeout') else 60 * 24 * 7
        self._background_chunk_index = 0

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

        TaskPool.Process.queue.send(TaskPool.Task(
            report_result, None, TaskPool.Process.name,
            f'{binary_name}.{test}',
            status,
            self._filter_noisy_output(output_buffer),
            elapsed=time.time() - started,
        ))

    def run_next_background_chunk(self, chunks):
        name, tests = chunks[self._background_chunk_index % len(chunks)]
        self._background_chunk_index += 1
        self.run(name, *tests)

    def run(self, name, *tests):
        binary_name = name.split('.')[0]
        remaining_tests = ['.'.join(test.split('.')[1:]) for test in tests]

        # Try to run the shard in a single process.
        while remaining_tests and not self._port.get_option('run_singly'):
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
