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


def setup_shard(port=None, devices=None):
    if devices and getattr(port, 'DEVICE_MANAGER', None):
        port.DEVICE_MANAGER.AVAILABLE_DEVICES = devices.get('available_devices', [])
        port.DEVICE_MANAGER.INITIALIZED_DEVICES = devices.get('initialized_devices', None)

    return _Worker.setup(port=port)


def run_shard(name, *tests):
    return _Worker.instance.run(name, *tests)


def report_result(worker, test, status, output):
    if status == Runner.STATUS_PASSED and (not output or Runner.instance.port.get_option('quiet')):
        Runner.instance.printer.write_update('{} {} {}'.format(worker, test, Runner.NAME_FOR_STATUS[status]))
    else:
        Runner.instance.printer.writeln('{} {} {}'.format(worker, test, Runner.NAME_FOR_STATUS[status]))
    Runner.instance.results[test] = status, output


def teardown_shard():
    return _Worker.teardown()


class Runner(object):
    STATUS_PASSED = 0
    STATUS_FAILED = 1
    STATUS_CRASHED = 2
    STATUS_TIMEOUT = 3
    STATUS_DISABLED = 4
    STATUS_RUNNING = 5

    NAME_FOR_STATUS = [
        'Passed',
        'Failed',
        'Crashed',
        'Timeout',
        'Disabled',
    ]

    instance = None

    def __init__(self, port, printer):
        self.port = port
        self.printer = printer
        self.tests_run = 0
        self._num_workers = 1
        self._has_logged_for_test = True  # Suppress an empty line between "Running tests" and the first test's output.
        self.results = {}

    # FIXME API tests should run as an app, we won't need this function <https://bugs.webkit.org/show_bug.cgi?id=175204>
    @staticmethod
    def command_for_port(port, args):
        if (port.get_option('force')):
            args.append('--gtest_also_run_disabled_tests=1')
        if getattr(port, 'DEVICE_MANAGER', None):
            assert port.DEVICE_MANAGER.INITIALIZED_DEVICES
            return ['/usr/bin/xcrun', 'simctl', 'spawn', port.DEVICE_MANAGER.INITIALIZED_DEVICES[0].udid] + args
        elif 'device' in port.port_name:
            raise RuntimeError('Running api tests on {} is not supported'.format(port.port_name))
        elif port.host.platform.is_win():
            args[0] = os.path.splitext(args[0])[0] + '.exe'
        return args

    @staticmethod
    def _shard_tests(tests):
        shards = {}
        for test in tests:
            shard_prefix = '.'.join(test.split('.')[:-1])
            if shard_prefix not in shards:
                shards[shard_prefix] = []
            shards[shard_prefix].append(test)
        return shards

    def run(self, tests, num_workers):
        if not tests:
            return

        self.printer.write_update('Sharding tests ...')
        shards = Runner._shard_tests(tests)

        original_level = server_process_logger.level
        server_process_logger.setLevel(logging.CRITICAL)

        try:
            if Runner.instance:
                raise RuntimeError('Cannot nest API test runners')
            Runner.instance = self
            self._num_workers = min(num_workers, len(shards))

            devices = None
            if getattr(self.port, 'DEVICE_MANAGER', None):
                devices = dict(
                    available_devices=self.port.DEVICE_MANAGER.AVAILABLE_DEVICES,
                    initialized_devices=self.port.DEVICE_MANAGER.INITIALIZED_DEVICES,
                )

            with TaskPool(
                workers=self._num_workers,
                setup=setup_shard, setupkwargs=dict(port=self.port, devices=devices), teardown=teardown_shard,
            ) as pool:
                for name, tests in iteritems(shards):
                    pool.do(run_shard, name, *tests)
                pool.wait()

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

    @classmethod
    def setup(cls, port=None):
        cls.instance = cls(port)

    @classmethod
    def teardown(cls):
        cls.instance = None

    def __init__(self, port):
        self._port = port
        self.host = port.host

        # ServerProcess doesn't allow for a timeout of 'None,' this uses a week instead of None.
        self._timeout = int(self._port.get_option('timeout')) if self._port.get_option('timeout') else 60 * 24 * 7

    @classmethod
    def _filter_noisy_output(cls, output):
        result = ''
        for line in output.splitlines():
            if line.lstrip().startswith('objc['):
                continue
            result += line + '\n'
        return result

    def _run_single_test(self, binary_name, test):
        server_process = ServerProcess(
            self._port, binary_name,
            Runner.command_for_port(self._port, [self._port._build_path(binary_name), '--gtest_filter={}'.format(test)]),
            env=self._port.environment_for_api_tests())

        status = Runner.STATUS_RUNNING
        if test.split('.')[1].startswith('DISABLED_') and not self._port.get_option('force'):
            status = Runner.STATUS_DISABLED

        stdout_buffer = ''
        stderr_buffer = ''

        try:
            deadline = time.time() + self._timeout
            if status != Runner.STATUS_DISABLED:
                server_process.start()

            while status == Runner.STATUS_RUNNING:
                stdout_line, stderr_line = server_process.read_either_stdout_or_stderr_line(deadline)
                if not stderr_line and not stdout_line:
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
            remaining_stderr = string_utils.decode(server_process.pop_all_buffered_stderr(), target_type=str)
            remaining_stdout = string_utils.decode(server_process.pop_all_buffered_stdout(), target_type=str)
            for line in (remaining_stdout + remaining_stderr).splitlines(False):
                _log.error(line)
            output_buffer = stderr_buffer + stdout_buffer + remaining_stderr + remaining_stdout
            server_process.stop()

        TaskPool.Process.queue.send(TaskPool.Task(
            report_result, None, TaskPool.Process.name,
            '{}.{}'.format(binary_name, test),
            status,
            self._filter_noisy_output(output_buffer),
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
                    self._port._build_path(binary_name), '--gtest_filter={}'.format(':'.join(remaining_tests))
                ]), env=self._port.environment_for_api_tests())

            try:
                deadline = time.time() + self._timeout
                last_test = None
                last_status = None
                stdout_buffer = ''

                server_process.start()
                while remaining_tests:
                    stdout = string_utils.decode(server_process.read_stdout_line(deadline), target_type=str)

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
                    if len(stdout_split) != 2 or not (stdout_split[0].startswith('**') and stdout_split[0].endswith('**')):
                        stdout_buffer += stdout
                        continue
                    if last_test is not None:
                        remaining_tests.remove(last_test)

                        for line in stdout_buffer.splitlines(False):
                            _log.error(line)
                        TaskPool.Process.queue.send(TaskPool.Task(
                            report_result, None, TaskPool.Process.name,
                            '{}.{}'.format(binary_name, last_test),
                            last_status, stdout_buffer,
                        ))
                        deadline = time.time() + self._timeout
                        stdout_buffer = ''

                    if '**PASS**' == stdout_split[0]:
                        last_status = Runner.STATUS_PASSED
                    else:
                        last_status = Runner.STATUS_FAILED
                    last_test = stdout_split[1]

                # We assume that stderr is only relevant if there is a crash (meaning we triggered an assert)
                if last_test:
                    remaining_tests.remove(last_test)
                    stdout_buffer += string_utils.decode(server_process.pop_all_buffered_stdout(), target_type=str)
                    stderr_buffer = string_utils.decode(server_process.pop_all_buffered_stderr(), target_type=str) if last_status == Runner.STATUS_CRASHED else ''
                    for line in (stdout_buffer + stderr_buffer).splitlines(keepends=False):
                        _log.error(line)

                    TaskPool.Process.queue.send(TaskPool.Task(
                        report_result, None, TaskPool.Process.name,
                        '{}.{}'.format(binary_name, last_test),
                        last_status,
                        self._filter_noisy_output(stdout_buffer + stderr_buffer),
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
