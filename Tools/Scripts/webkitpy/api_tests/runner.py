# Copyright (C) 2018 Apple Inc. All rights reserved.
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
import time

from webkitpy.common import message_pool
from webkitpy.port.server_process import ServerProcess
from webkitpy.xcode.simulated_device import SimulatedDeviceManager


class Runner(object):
    STATUS_PASSED = 0
    STATUS_FAILED = 1
    STATUS_CRASHED = 2
    STATUS_TIMEOUT = 3
    STATUS_DISABLED = 4

    NAME_FOR_STATUS = [
        'Passed',
        'Failed',
        'Crashed',
        'Timeout',
        'Disabled',
    ]

    def __init__(self, port, printer):
        self.port = port
        self.printer = printer
        self.tests_run = 0
        self.results = {}

    @staticmethod
    def _shard_tests(tests):
        shards = {}
        for test in tests:
            shard_prefix = '.'.join(test.split('.')[:-1])
            if shard_prefix not in shards:
                shards[shard_prefix] = []
            shards[shard_prefix].append(test)
        return shards

    # FIXME API tests should run as an app, we won't need this function <https://bugs.webkit.org/show_bug.cgi?id=175204>
    @staticmethod
    def command_for_port(port, args):
        if 'simulator' in port.port_name:
            assert SimulatedDeviceManager.INITIALIZED_DEVICES
            return ['/usr/bin/xcrun', 'simctl', 'spawn', SimulatedDeviceManager.INITIALIZED_DEVICES[0].udid] + args
        elif 'device' in port.port_name:
            raise RuntimeError('Running api tests on {} is not supported'.format(port.port_name))
        elif port.host.platform.is_win():
            args[0] = os.path.splitext(args[0])[0] + '.exe'
        return args

    def run(self, tests, num_workers):
        if not tests:
            return

        self.printer.write_update('Sharding tests ...')
        shards = Runner._shard_tests(tests)

        with message_pool.get(self, lambda caller: _Worker(caller, self.port, shards), min(num_workers, len(shards))) as pool:
            pool.run(('test', shard) for shard, _ in shards.iteritems())

    def handle(self, message_name, source, test_name=None, status=0, output=''):
        if message_name == 'did_spawn_worker':
            return
        if message_name == 'ended_test':
            update = '{} {} {}'.format(source, test_name, Runner.NAME_FOR_STATUS[status])

            # Don't print test output if --quiet.
            if status != Runner.STATUS_PASSED or (output and not self.port.get_option('quiet')):
                self.printer.writeln(update)
                for line in output.splitlines():
                    self.printer.writeln('    {}'.format(line))
            else:
                self.printer.write_update(update)
            self.tests_run += 1
            self.results[test_name] = (status, output)

    def result_map_by_status(self, status=None):
        map = {}
        for test_name, result in self.results.iteritems():
            if result[0] == status:
                map[test_name] = result[1]
        return map


class _Worker(object):
    def __init__(self, caller, port, shard_map):
        self._caller = caller
        self._port = port
        self.host = port.host
        self._shard_map = shard_map

        # ServerProcess doesn't allow for a timeout of 'None,' this uses a week instead of None.
        self._timeout = int(self._port.get_option('timeout')) if self._port.get_option('timeout') else 60 * 24 * 7

    @staticmethod
    def _filter_noisy_output(output):
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

        try:
            deadline = time.time() + self._timeout
            server_process.start()

            if not test.split('.')[1].startswith('DISABLED_'):
                stdout_line = server_process.read_stdout_line(deadline)
            else:
                stdout_line = None

            if not stdout_line and server_process.timed_out:
                status = Runner.STATUS_TIMEOUT
            elif not stdout_line and server_process.has_crashed():
                status = Runner.STATUS_CRASHED
            elif not stdout_line:
                status = Runner.STATUS_DISABLED
            elif '**PASS**' in stdout_line:
                status = Runner.STATUS_PASSED
            else:
                status = Runner.STATUS_FAILED

        finally:
            output_buffer = server_process.pop_all_buffered_stdout() + server_process.pop_all_buffered_stderr()
            server_process.stop()

        self._caller.post('ended_test', '{}.{}'.format(binary_name, test), status, self._filter_noisy_output(output_buffer))

    def _run_shard_with_binary(self, binary_name, tests):
        remaining_tests = list(tests)

        # Try to run the shard in a single process.
        while remaining_tests and not self._port.get_option('run_singly'):
            starting_length = len(remaining_tests)
            server_process = ServerProcess(
                self._port, binary_name,
                Runner.command_for_port(self._port, [self._port._build_path(binary_name), '--gtest_filter={}'.format(':'.join(remaining_tests))]),
                env=self._port.environment_for_api_tests())

            try:
                deadline = time.time() + self._timeout
                last_test = None
                last_status = None
                stdout_buffer = ''

                server_process.start()
                while remaining_tests:
                    stdout = server_process.read_stdout_line(deadline)

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
                        self._caller.post('ended_test', '{}.{}'.format(binary_name, last_test), last_status, stdout_buffer)
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
                    stdout_buffer += server_process.pop_all_buffered_stdout()
                    stderr_buffer = server_process.pop_all_buffered_stderr() if last_status == Runner.STATUS_CRASHED else ''
                    self._caller.post('ended_test', '{}.{}'.format(binary_name, last_test), last_status, self._filter_noisy_output(stdout_buffer + stderr_buffer))

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

    def handle(self, message_name, source, shard_name):
        assert message_name == 'test'
        self._caller.post('started_shard', shard_name)

        binary_map = {}
        for test in self._shard_map[shard_name]:
            split_test_name = test.split('.')
            if split_test_name[0] not in binary_map:
                binary_map[split_test_name[0]] = []
            binary_map[split_test_name[0]].append('.'.join(split_test_name[1:]))
        for binary_name, test_list in binary_map.iteritems():
            self._run_shard_with_binary(binary_name, test_list)
