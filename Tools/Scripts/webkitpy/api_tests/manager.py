# Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

import json
import logging
import os
import re
import time

from webkitpy.api_tests.runner import Runner
from webkitpy.common.iteration_compatibility import iteritems
from webkitpy.common.system.executive import ScriptError
from webkitpy.results.upload import Upload
from webkitpy.xcode.simulated_device import DeviceRequest, SimulatedDeviceManager

_log = logging.getLogger(__name__)


class Manager(object):
    """A class for managing running API and WTF tests
    """

    SUCCESS = 0
    FAILED_BUILD_CHECK = 1
    FAILED_COLLECT_TESTS = 2
    FAILED_TESTS = 3
    FAILED_UPLOAD = 4

    def __init__(self, port, options, stream):
        self._port = port
        self.host = port.host
        self._options = options
        self._stream = stream

    @staticmethod
    def _test_list_from_output(output, prefix=''):
        result = []
        current_test_suite = None
        for line in output.split('\n'):
            line = line.split("#")[0]  # Value-parametrized tests contain #.
            striped_line = line.lstrip().rstrip()
            if not striped_line:
                continue

            if striped_line[-1] == '.':
                current_test_suite = striped_line[:-1]
            else:
                striped_line = striped_line.lstrip()
                if ' ' in striped_line:
                    continue
                val = f'{prefix}{current_test_suite}.{striped_line}'
                if val not in result:
                    result.append(val)
        return result

    @staticmethod
    def _find_test_subset(superset, arg_filter):
        result = []
        for arg in arg_filter:
            # Might match <binary>.<suite>.<test> or just <suite>.<test>
            arg_re = re.compile(f'^{arg.replace("*", ".*")}$')
            for test in superset:
                if arg_re.match(test):
                    result.append(test)
                    continue

                split_test = test.split('.')
                if len(split_test) == 1:
                    continue
                if arg_re.match('.'.join(split_test[1:])):
                    result.append(test)
                    continue
                if arg_re.match('.'.join(split_test[:-1])):
                    result.append(test)
                    continue

                if len(split_test) == 2:
                    continue
                if arg_re.match('.'.join(split_test[1:-1])):
                    result.append(test)
                    continue
        return result

    def _collect_tests(self, args):
        available_tests = []
        specified_binaries = self._binaries_for_arguments(args)
        for canonicalized_binary, path in self._port.path_to_api_test_binaries().items():
            if canonicalized_binary not in specified_binaries:
                continue

            to_be_listed = path
            if not self._port.host.platform.is_win():
                to_be_listed = self.host.filesystem.join(self.host.filesystem.dirname(path), 'ToBeListed')
                self.host.filesystem.copyfile(path, to_be_listed)
                self.host.filesystem.copymode(path, to_be_listed)
            try:
                output = self.host.executive.run_command(
                    Runner.command_for_port(self._port, [to_be_listed, '--gtest_list_tests']),
                    env=self._port.environment_for_api_tests())
                available_tests += Manager._test_list_from_output(output, f'{canonicalized_binary}.')
            except ScriptError:
                _log.error(f'Failed to list {canonicalized_binary} tests')
                raise
            finally:
                if not self._port.host.platform.is_win():
                    self.host.filesystem.remove(to_be_listed)

        if len(args) == 0:
            return sorted(available_tests)
        return sorted(Manager._find_test_subset(available_tests, args))

    @staticmethod
    def _print_test_result(stream, test_name, output):
        stream.writeln(f'    {test_name}')
        has_output = False
        for line in output.splitlines():
            stream.writeln(f'        {line}')
            has_output = True
        if has_output:
            stream.writeln('')
        return not has_output

    def _print_tests_result_with_status(self, status, runner):
        mapping = runner.result_map_by_status(status)
        if mapping:
            self._stream.writeln(runner.NAME_FOR_STATUS[status])
            self._stream.writeln('')
            need_newline = False
            for test, output in iteritems(mapping):
                need_newline = Manager._print_test_result(self._stream, test, output)
            if need_newline:
                self._stream.writeln('')


    def _binaries_for_arguments(self, args):
        if self._port.get_option('api_binary'):
            return self._port.get_option('api_binary')

        binaries = []
        for arg in args:
            candidate_binary = arg.split('.')[0]
            if candidate_binary in binaries:
                continue
            if candidate_binary in self._port.path_to_api_test_binaries():
                binaries.append(candidate_binary)
            else:
                # If the user specifies a test-name without a binary, we need to search both binaries
                return self._port.path_to_api_test_binaries().keys()
        return binaries or self._port.path_to_api_test_binaries().keys()

    def _update_worker_count(self):
        child_processes_option_value = int(self._options.child_processes or 0)
        specified_child_processes = (
            child_processes_option_value
            or self._port.default_child_processes()
        )
        self._options.child_processes = specified_child_processes

    def _set_up_run(self, device_type=None):
        self._stream.write_update("Starting helper ...")
        if not self._port.start_helper():
            return False

        self._update_worker_count()
        self._port.reset_preferences()

        # Set up devices for the test run
        if 'simulator' in self._port.port_name:
            if device_type is None:
                device_type = self._port.supported_device_types()[0]
            self._port.setup_test_run(device_type=device_type)
        elif 'device' in self._port.port_name:
            raise RuntimeError(f'Running api tests on {self._port.port_name} is not supported')

        return True

    def _clean_up_run(self):
        """Clean up the test run."""
        self._port.stop_helper()
        self._port.clean_up_test_run()

    def run(self, args, json_output=None):
        if json_output:
            json_output = self.host.filesystem.abspath(json_output)
            if not self.host.filesystem.isdir(self.host.filesystem.dirname(json_output)) or self.host.filesystem.isdir(json_output):
                raise RuntimeError(f'Cannot write to {json_output}')

        start_time = time.time()

        self._stream.write_update('Checking build ...')
        if not self._port.check_api_test_build(self._binaries_for_arguments(args)):
            _log.error('Build check failed')
            return Manager.FAILED_BUILD_CHECK

        if not self._set_up_run():
            return Manager.FAILED_BUILD_CHECK

        configuration_for_upload = self._port.configuration_for_upload(self._port.target_host(0))

        self._stream.write_update('Collecting tests ...')
        try:
            test_names = self._collect_tests(args)
        except ScriptError:
            self._stream.writeln('Failed to collect tests')
            return Manager.FAILED_COLLECT_TESTS
        self._stream.write_update(f'Found {len(test_names)} tests')
        if len(test_names) == 0:
            self._stream.writeln('No tests found')
            return Manager.FAILED_COLLECT_TESTS

        if self._port.get_option('dump'):
            for test in test_names:
                self._stream.writeln(test)
            return Manager.SUCCESS

        test_names = [test for test in test_names for _ in range(self._options.repeat_each)]
        if self._options.repeat_each != 1:
            _log.debug(f'Repeating each test {self._options.iterations} times')

        runner = None
        try:
            _log.info('Running tests')
            runner = Runner(self._port, self._stream)
            for i in range(self._options.iterations):
                _log.debug(f'\nIteration {i + 1}')
                runner.run(test_names, int(self._options.child_processes) if self._options.child_processes else None)
        except KeyboardInterrupt:
            # If we receive a KeyboardInterrupt, print results.
            self._stream.writeln('')
        finally:
            self._clean_up_run()

        end_time = time.time()

        if not runner:
            return Manager.FAILED_TESTS

        successful = runner.result_map_by_status(runner.STATUS_PASSED)
        disabled = len(runner.result_map_by_status(runner.STATUS_DISABLED))

        # Check if running in test-parallel-safety mode
        test_parallel_safety_tests = self._port.get_option('test_parallel_safety') or []
        is_parallel_safety_mode = bool(test_parallel_safety_tests)

        _log.info(f'Ran {len(runner.results) - disabled} tests of {len(set(test_names))} with {len(successful)} successful')

        result_dictionary = {
            'Skipped': [],
            'Failed': [],
            'Crashed': [],
            'Timedout': [],
        }

        self._stream.writeln('-' * 30)
        result = Manager.SUCCESS

        # Count actual failures (not skipped)
        failed_tests = runner.result_map_by_status(runner.STATUS_FAILED)
        crashed_tests = runner.result_map_by_status(runner.STATUS_CRASHED)
        timedout_tests = runner.result_map_by_status(runner.STATUS_TIMEOUT)
        has_real_failures = bool(failed_tests or crashed_tests or timedout_tests)

        if is_parallel_safety_mode:
            # In parallel-safety mode, skipped tests are expected
            # Only fail if there are actual failures, crashes, or timeouts
            if has_real_failures:
                self._stream.writeln('Test suite failed')
                result = Manager.FAILED_TESTS
            else:
                self._stream.writeln('All parallel-safety tests passed!')
                if json_output:
                    self.host.filesystem.write_text_file(json_output, json.dumps(result_dictionary, indent=4))
        elif len(successful) + disabled == len(set(test_names)):
            self._stream.writeln('All tests successfully passed!')
            if json_output:
                self.host.filesystem.write_text_file(json_output, json.dumps(result_dictionary, indent=4))
        else:
            self._stream.writeln('Test suite failed')
            result = Manager.FAILED_TESTS

        # Always collect skipped/failed info for reporting
        if result != Manager.SUCCESS or is_parallel_safety_mode:
            self._stream.writeln('')

            skipped = []
            for test in test_names:
                if test not in runner.results:
                    skipped.append(test)
                    result_dictionary['Skipped'].append({'name': test, 'output': None})
            if skipped:
                self._stream.writeln(f'Skipped {len(skipped)} tests')
                self._stream.writeln('')
                if self._options.verbose:
                    for test in skipped:
                        self._stream.writeln(f'    {test}')

            self._print_tests_result_with_status(runner.STATUS_FAILED, runner)
            self._print_tests_result_with_status(runner.STATUS_CRASHED, runner)
            self._print_tests_result_with_status(runner.STATUS_TIMEOUT, runner)

            for test, test_result in iteritems(runner.results):
                status_to_string = {
                    runner.STATUS_FAILED: 'Failed',
                    runner.STATUS_CRASHED: 'Crashed',
                    runner.STATUS_TIMEOUT: 'Timedout',
                }.get(test_result[0])
                if not status_to_string:
                    continue
                result_dictionary[status_to_string].append({'name': test, 'output': test_result[1]})

            if json_output:
                self.host.filesystem.write_text_file(json_output, json.dumps(result_dictionary, indent=4))

        if self._options.report_urls:
            self._stream.writeln('\n')
            self._stream.write_update('Preparing upload data ...')

            status_to_test_result = {
                runner.STATUS_PASSED: None,
                runner.STATUS_FAILED: Upload.Expectations.FAIL,
                runner.STATUS_CRASHED: Upload.Expectations.CRASH,
                runner.STATUS_TIMEOUT: Upload.Expectations.TIMEOUT,
            }
            upload = Upload(
                suite=self._options.suite or 'api-tests',
                configuration=configuration_for_upload,
                details=Upload.create_details(options=self._options),
                commits=self._port.commits_for_upload(),
                run_stats=Upload.create_run_stats(
                    start_time=start_time,
                    end_time=end_time,
                    tests_skipped=len(result_dictionary['Skipped']),
                ), results={
                    test: Upload.create_test_result(
                        actual=status_to_test_result[result[0]],
                        time=int(result[2] * 1000),
                    ) for test, result in iteritems(runner.results) if result[0] in status_to_test_result
                },
            )
            for url in self._options.report_urls:
                self._stream.write_update(f'Uploading to {url} ...')
                if not upload.upload(url, log_line_func=self._stream.writeln):
                    result = Manager.FAILED_UPLOAD
            self._stream.writeln('Uploads completed!')

        return result
