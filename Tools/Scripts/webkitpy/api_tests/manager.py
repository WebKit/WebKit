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
from webkitpy.api_tests.test_expectations import (
    APITestExpectations, PASS, FAIL, CRASH, TIMEOUT,
    runner_status_to_expectation,
)
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
        self._expectations = None

    @staticmethod
    def _test_list_from_output(output: str, prefix='') -> list[str]:
        result = []
        current_test_suite = None
        for line in output.split('\n'):
            if prefix and line.startswith(prefix) and '/' in line:
                # this implies it's a Swift Test
                result.append(line)
            else:
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
            escaped = re.escape(arg).replace(r'\*', '.*')
            arg_re = re.compile(f'^{escaped}$')
            for test in superset:
                if arg_re.match(test):
                    result.append(test)
                    continue

                first_dot = test.find('.')
                if first_dot == -1:
                    continue

                # Find the last separator (either '.' or '/')
                last_dot = test.rfind('.')
                last_slash = test.rfind('/')
                last_sep = max(last_dot, last_slash)

                # Without binary prefix
                without_binary = test[first_dot + 1:]
                if arg_re.match(without_binary):
                    result.append(test)
                    continue

                # Without test function suffix
                if last_sep > first_dot:
                    without_test = test[:last_sep]
                    if arg_re.match(without_test):
                        result.append(test)
                        continue

                    # Just the suite (middle part)
                    suite_only = test[first_dot + 1:last_sep]
                    if arg_re.match(suite_only):
                        result.append(test)
                        continue
        return result

    @staticmethod
    def _expected_results_for_upload(expectation):
        """Return the results-database `expected` string for an API-test expectation.

        None means "expected to pass" (uploaded as a default pass); otherwise a
        space-separated list of expected result states, so gardened and flaky tests
        are not reported as unexpected failures on results.webkit.org.
        """
        if expectation is None or expectation.expected == PASS:
            return None
        status_to_upload_text = {
            PASS: Upload.Expectations.PASS,
            FAIL: Upload.Expectations.FAIL,
            CRASH: Upload.Expectations.CRASH,
            TIMEOUT: Upload.Expectations.TIMEOUT,
        }
        return ' '.join(text for status, text in status_to_upload_text.items() if status in expectation.expected) or None

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
                    Runner.command_for_port(self._port, [to_be_listed, '--list-tests']),
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

    def _load_expectations(self, test_names):
        self._expectations = APITestExpectations(self._port, tests=test_names)
        self._expectations.parse_all_expectations()

        additional_expectations = getattr(self._options, 'additional_expectations', []) or []
        for filepath in additional_expectations:
            self._expectations.parse_additional_file(filepath)

        return self._expectations

    _SPECIFIC_TEST_ARG_RE = re.compile(r'\S+\..+')

    @classmethod
    def _args_specify_individual_tests(cls, args):
        return bool(args) and all(cls._SPECIFIC_TEST_ARG_RE.match(arg) for arg in args)

    def _update_worker_count(self, args):
        child_processes_option_value = int(self._options.child_processes or 0)
        if not child_processes_option_value and self._args_specify_individual_tests(args):
            # When the user names specific tests (e.g. Foo.Bar), avoid booting
            # one simulator per CPU just to run a handful of tests.
            _log.info('All arguments name specific tests; defaulting to --child-processes=1')
            self._options.child_processes = 1
            return
        self._options.child_processes = (
            child_processes_option_value
            or self._port.default_child_processes()
        )

    def _set_up_run(self, args, device_type=None):
        self._stream.write_update("Starting helper ...")
        if not self._port.start_helper():
            return False

        self._update_worker_count(args)
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

    @staticmethod
    def _read_test_names_from_file(filenames: list[str], filesystem) -> list[str]:
        result = []
        for filename in filenames:
            with filesystem.open_text_file_for_reading(filename) as f:
                for line in f:
                    line = line.split('#')[0].strip()
                    if line:
                        result.append(line)
        return result

    def run(self, args, json_output=None):
        if self._options.test_list:
            for list_path in self._options.test_list:
                if not self.host.filesystem.isfile(list_path):
                    _log.error('--test-list file "%s" not found', list_path)
                    return Manager.FAILED_COLLECT_TESTS
            args = args + Manager._read_test_names_from_file(self._options.test_list, self.host.filesystem)

        if json_output:
            json_output = self.host.filesystem.abspath(json_output)
            if not self.host.filesystem.isdir(self.host.filesystem.dirname(json_output)) or self.host.filesystem.isdir(json_output):
                raise RuntimeError(f'Cannot write to {json_output}')

        start_time = time.time()

        self._stream.write_update('Checking build ...')
        if not self._port.check_api_test_build(self._binaries_for_arguments(args)):
            _log.error('Build check failed')
            return Manager.FAILED_BUILD_CHECK

        if not self._set_up_run(args):
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

        self._stream.write_update('Loading test expectations ...')
        self._load_expectations(test_names)
        current_config = self._expectations.get_current_configuration()

        original_test_count = len(test_names)
        skip_failing_tests = getattr(self._options, 'skip_failing_tests', False)
        skip_flaky_tests = getattr(self._options, 'skip_flaky_tests', False)
        skipped_by_expectation = set()
        skipped_as_failing = set()
        skipped_as_flaky = set()

        skipped_option = getattr(self._options, 'skipped', 'default')

        for test in test_names:
            exp = self._expectations.get_expectation(test, current_config)
            if not exp:
                continue

            if skipped_option != 'ignore' and exp.is_skip():
                skipped_by_expectation.add(test)
                continue

            if skip_failing_tests and (exp.is_flaky() or FAIL in exp.expected or CRASH in exp.expected or TIMEOUT in exp.expected):
                skipped_as_failing.add(test)
            elif skip_flaky_tests and exp.is_flaky():
                skipped_as_flaky.add(test)

        if skipped_option == 'only':
            test_names = [t for t in test_names if t in skipped_by_expectation]
            skipped_by_expectation = set()
        else:
            all_skipped = skipped_by_expectation | skipped_as_failing | skipped_as_flaky
            test_names = [t for t in test_names if t not in all_skipped]

        if skipped_by_expectation:
            self._stream.write_update(f'Skipping {len(skipped_by_expectation)} tests marked [ Skip ]')
        if skipped_as_failing:
            self._stream.write_update(f'Skipping {len(skipped_as_failing)} tests marked as failing (--skip-failing-tests)')
        if skipped_as_flaky:
            self._stream.write_update(f'Skipping {len(skipped_as_flaky)} flaky tests (--skip-flaky-tests)')

        test_names = [test for test in test_names for _ in range(self._options.repeat_each)]
        if self._options.repeat_each != 1:
            _log.debug(f'Repeating each test {self._options.iterations} times')

        runner = None
        try:
            _log.info('Running tests')
            runner = Runner(self._port, self._stream, expectations=self._expectations)
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

        unexpected_failures = {}
        expected_failures = {}
        unexpected_passes = {}

        for test, test_result in iteritems(runner.results):
            status = test_result[0]
            if status == runner.STATUS_DISABLED:
                continue
            exp = self._expectations.get_expectation(test, current_config)
            expectation_status = runner_status_to_expectation(status)

            if exp is None or exp.expected == PASS:
                if status != runner.STATUS_PASSED:
                    unexpected_failures[test] = test_result
            else:
                is_expected = exp.result_is_expected(expectation_status) if expectation_status is not None else False
                if is_expected:
                    if status != runner.STATUS_PASSED:
                        expected_failures[test] = test_result
                else:
                    if status == runner.STATUS_PASSED:
                        unexpected_passes[test] = test_result
                    else:
                        unexpected_failures[test] = test_result

        summary = f'Ran {len(runner.results) - disabled} tests of {original_test_count} with {len(successful)} successful'
        if expected_failures:
            summary += f' ({len(expected_failures)} expected failures)'
        _log.info(summary)

        result_dictionary = {
            'Skipped': [],
            'Failed': [],
            'Crashed': [],
            'Timedout': [],
            'UnexpectedFailures': [],
            'ExpectedFailures': [],
            'UnexpectedPasses': [],
        }

        self._stream.writeln('-' * 30)
        result = Manager.SUCCESS

        has_unexpected_failures = bool(unexpected_failures)

        if is_parallel_safety_mode:
            if has_unexpected_failures:
                self._stream.writeln('Test suite failed')
                result = Manager.FAILED_TESTS
            else:
                self._stream.writeln('All parallel-safety tests passed!')
                if json_output:
                    self.host.filesystem.write_text_file(json_output, json.dumps(result_dictionary, indent=4))
        elif not has_unexpected_failures:
            if expected_failures:
                self._stream.writeln(f'All tests passed! ({len(expected_failures)} expected failures)')
            else:
                self._stream.writeln('All tests successfully passed!')
            if json_output:
                self.host.filesystem.write_text_file(json_output, json.dumps(result_dictionary, indent=4))
        else:
            self._stream.writeln('Test suite failed')
            result = Manager.FAILED_TESTS

        self._stream.writeln('')

        all_skipped_tests = list(skipped_by_expectation | skipped_as_failing | skipped_as_flaky)
        for test in all_skipped_tests:
            result_dictionary['Skipped'].append({'name': test, 'output': None})
        if all_skipped_tests:
            self._stream.writeln(f'Skipped {len(all_skipped_tests)} tests')
            self._stream.writeln('')

        if unexpected_failures:
            self._stream.writeln('** UNEXPECTED FAILURES **')
            self._stream.writeln('')
            for test, test_result in iteritems(unexpected_failures):
                Manager._print_test_result(self._stream, test, test_result[1])
                status_str = {
                    runner.STATUS_FAILED: 'Failed',
                    runner.STATUS_CRASHED: 'Crashed',
                    runner.STATUS_TIMEOUT: 'Timedout',
                }.get(test_result[0], 'Failed')
                result_dictionary['UnexpectedFailures'].append({'name': test, 'output': test_result[1], 'status': status_str})
                result_dictionary[status_str].append({'name': test, 'output': test_result[1]})

        if expected_failures:
            self._stream.writeln('Expected failures (not blocking):')
            for test in expected_failures:
                self._stream.writeln(f'    {test}')
                test_result = expected_failures[test]
                result_dictionary['ExpectedFailures'].append({'name': test, 'output': test_result[1]})
            self._stream.writeln('')

        if unexpected_passes:
            self._stream.writeln('Unexpected passes (may need expectation update):')
            for test in unexpected_passes:
                self._stream.writeln(f'    {test}')
                result_dictionary['UnexpectedPasses'].append({'name': test})
            self._stream.writeln('')

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
            upload_results = {}
            for test, result in iteritems(runner.results):
                if result[0] not in status_to_test_result:
                    continue
                upload_results[test] = Upload.create_test_result(
                    expected=self._expected_results_for_upload(self._expectations.get_expectation(test, current_config)),
                    actual=status_to_test_result[result[0]],
                    time=int(result[2] * 1000),
                )
            upload = Upload(
                suite=self._options.suite or 'api-tests',
                configuration=configuration_for_upload,
                details=Upload.create_details(options=self._options),
                commits=self._port.commits_for_upload(),
                run_stats=Upload.create_run_stats(
                    start_time=start_time,
                    end_time=end_time,
                    tests_skipped=len(result_dictionary['Skipped']),
                ),
                results=upload_results,
            )
            for url in self._options.report_urls:
                self._stream.write_update(f'Uploading to {url} ...')
                if not upload.upload(url, log_line_func=self._stream.writeln):
                    result = Manager.FAILED_UPLOAD
            self._stream.writeln('Uploads completed!')

        return result
