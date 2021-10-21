# -*- coding: UTF-8 -*-

# Copyright (C) 2010 Google Inc. All rights reserved.
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

"""
The Manager runs a series of tests (TestType interface) against a set
of test files.  If a test file fails a TestType, it returns a list of TestFailure
objects to the Manager. The Manager then aggregates the TestFailures to
create a final report.
"""

import csv
import json
import logging
import os
import random
import re
import shutil
import sys
import time
from collections import defaultdict, OrderedDict

from webkitcorepy.string_utils import pluralize

from webkitpy.common.iteration_compatibility import iteritems, itervalues
from webkitpy.layout_tests.controllers.layout_test_finder_legacy import LayoutTestFinder
from webkitpy.layout_tests.controllers.layout_test_runner import LayoutTestRunner
from webkitpy.layout_tests.controllers.test_result_writer import TestResultWriter
from webkitpy.layout_tests.layout_package import json_layout_results_generator
from webkitpy.layout_tests.layout_package import json_results_generator
from webkitpy.layout_tests.models import test_expectations
from webkitpy.layout_tests.models import test_failures
from webkitpy.layout_tests.models import test_results
from webkitpy.layout_tests.models import test_run_results
from webkitpy.layout_tests.models.test_input import TestInput
from webkitpy.layout_tests.models.test_run_results import INTERRUPTED_EXIT_STATUS, TestRunResults
from webkitpy.results.upload import Upload
from webkitpy.xcode.device_type import DeviceType

_log = logging.getLogger(__name__)

TestExpectations = test_expectations.TestExpectations


class Manager(object):
    """Test execution manager

    This class has the main entry points for run-webkit-tests; the ..run_webkit_tests module almost
    exclusively just handles CLI options. It orchestrates collecting the tests (through
    LayoutTestFinder), running them (LayoutTestRunner), and then displaying the results
    (TestResultWriter/Printer).
    """

    def __init__(self, port, options, printer):
        """Initialize test runner data structures.

        Args:
          port: an object implementing port-specific
          options: a dictionary of command line options
          printer: a Printer object to record updates to.
        """
        self._port = port
        fs = port.host.filesystem
        self._filesystem = fs
        self._options = options
        self._printer = printer
        self._expectations = OrderedDict()
        self._results_directory = self._port.results_directory()
        self._finder = LayoutTestFinder(self._port, self._options)
        self._runner = None

        self._tests_options = {}
        test_options_json_path = fs.join(self._port.layout_tests_dir(), "tests-options.json")
        if fs.exists(test_options_json_path):
            with fs.open_binary_file_for_reading(test_options_json_path) as fd:
                try:
                    self._tests_options = json.load(fd)
                except (ValueError, IOError):
                    pass

    def _collect_tests(self,
                       paths,  # type: List[str]
                       device_type_list,  # type: List[Optional[DeviceType]]
                       ):
        aggregate_tests = set()  # type: Set[Test]
        aggregate_tests_to_run = set()  # type: Set[Test]
        tests_to_run_by_device = {}  # type: Dict[Optional[DeviceType], List[Test]]

        device_type_list = self._port.supported_device_types()
        for device_type in device_type_list:
            for_device_type = u'for {} '.format(device_type) if device_type else ''
            self._printer.write_update(u'Collecting tests {}...'.format(for_device_type))
            paths, tests = self._finder.find_tests(self._options, paths, device_type=device_type)
            aggregate_tests.update(tests)

            test_names = [test.test_path for test in tests]

            self._printer.write_update(u'Parsing expectations {}...'.format(for_device_type))
            self._expectations[device_type] = test_expectations.TestExpectations(self._port, test_names, force_expectations_pass=self._options.force, device_type=device_type)
            self._expectations[device_type].parse_all_expectations()

            tests_to_run = self._tests_to_run(tests, device_type=device_type)
            tests_to_run_by_device[device_type] = [test for test in tests_to_run if test not in aggregate_tests_to_run]
            aggregate_tests_to_run.update(tests_to_run_by_device[device_type])

        aggregate_tests_to_skip = aggregate_tests - aggregate_tests_to_run

        return tests_to_run_by_device, aggregate_tests_to_skip

    def _skip_tests(self, all_tests_list, expectations, http_tests):
        all_tests = set(all_tests_list)

        tests_to_skip = expectations.model().get_tests_with_result_type(test_expectations.SKIP)
        if self._options.skip_failing_tests:
            tests_to_skip.update(expectations.model().get_tests_with_result_type(test_expectations.FAIL))
            tests_to_skip.update(expectations.model().get_tests_with_result_type(test_expectations.FLAKY))

        if self._options.skipped == 'only':
            tests_to_skip = all_tests - tests_to_skip
        elif self._options.skipped == 'ignore':
            tests_to_skip = set()

        # unless of course we don't want to run the HTTP tests :)
        if not self._options.http:
            tests_to_skip.update(set(http_tests))

        return tests_to_skip

    def _split_into_chunks(self, test_names):
        """split into a list to run and a set to skip, based on --run-chunk and --run-part."""
        if not self._options.run_chunk and not self._options.run_part:
            return test_names, set()

        # If the user specifies they just want to run a subset of the tests,
        # just grab a subset of the non-skipped tests.
        chunk_value = self._options.run_chunk or self._options.run_part
        try:
            (chunk_num, chunk_len) = chunk_value.split(":")
            chunk_num = int(chunk_num)
            assert(chunk_num >= 0)
            test_size = int(chunk_len)
            assert(test_size > 0)
        except AssertionError:
            _log.critical("invalid chunk '%s'" % chunk_value)
            return (None, None)

        # Get the number of tests
        num_tests = len(test_names)

        # Get the start offset of the slice.
        if self._options.run_chunk:
            chunk_len = test_size
            # In this case chunk_num can be really large. We need
            # to make the worker fit in the current number of tests.
            slice_start = (chunk_num * chunk_len) % num_tests
        else:
            # Validate the data.
            assert(test_size <= num_tests)
            assert(chunk_num <= test_size)

            # To count the chunk_len, and make sure we don't skip
            # some tests, we round to the next value that fits exactly
            # all the parts.
            rounded_tests = num_tests
            if rounded_tests % test_size != 0:
                rounded_tests = (num_tests + test_size - (num_tests % test_size))

            chunk_len = rounded_tests // test_size
            slice_start = chunk_len * (chunk_num - 1)
            # It does not mind if we go over test_size.

        # Get the end offset of the slice.
        slice_end = min(num_tests, slice_start + chunk_len)

        tests_to_run = test_names[slice_start:slice_end]

        _log.debug('chunk slice [%d:%d] of %d is %d tests' % (slice_start, slice_end, num_tests, (slice_end - slice_start)))

        # If we reached the end and we don't have enough tests, we run some
        # from the beginning.
        if slice_end - slice_start < chunk_len:
            extra = chunk_len - (slice_end - slice_start)
            _log.debug('   last chunk is partial, appending [0:%d]' % extra)
            tests_to_run.extend(test_names[0:extra])

        return (tests_to_run, set(test_names) - set(tests_to_run))

    def _tests_to_run(self, tests, device_type):
        test_names = {test.test_path for test in tests}
        test_names_to_skip = self._skip_tests(test_names,
                                              self._expectations[device_type],
                                              {test.test_path for test in tests if test.needs_any_server})
        tests_to_run = [test for test in tests if test.test_path not in test_names_to_skip]

        # Create a sorted list of test files so the subset chunk,
        # if used, contains alphabetically consecutive tests.
        if self._options.order == 'natural':
            tests_to_run.sort(key=lambda x: self._port.test_key(x.test_path))
        elif self._options.order == 'random':
            random.shuffle(tests_to_run)

        tests_to_run, _ = self._split_into_chunks(tests_to_run)
        return tests_to_run

    def _test_input_for_file(self, test_file, device_type):
        test_is_slow = self._test_is_slow(test_file.test_path, device_type=device_type)
        reference_files = self._port.reference_files(
            test_file.test_path, device_type=device_type
        )
        timeout = (
            self._options.slow_time_out_ms
            if test_is_slow
            else self._options.time_out_ms
        )
        should_dump_jsconsolelog_in_stderr = (
            self._test_should_dump_jsconsolelog_in_stderr(
                test_file.test_path, device_type=device_type
            )
        )

        if reference_files:
            should_run_pixel_test = True
        elif not self._options.pixel_tests:
            should_run_pixel_test = False
        elif self._options.pixel_test_directories:
            should_run_pixel_test = any(
                test_file.test_path.startswith(directory)
                for directory in self._options.pixel_test_directories
            )
        else:
            should_run_pixel_test = True

        return TestInput(
            test_file,
            timeout=timeout,
            is_slow=test_is_slow,
            needs_servers=test_file.needs_any_server,
            should_dump_jsconsolelog_in_stderr=should_dump_jsconsolelog_in_stderr,
            reference_files=reference_files,
            should_run_pixel_test=should_run_pixel_test,
        )

    def _test_is_slow(self, test_file, device_type):
        if self._expectations[device_type].model().has_modifier(test_file, test_expectations.SLOW):
            return True
        return "slow" in self._tests_options.get(test_file, [])

    def _test_should_dump_jsconsolelog_in_stderr(self, test_file, device_type):
        return self._expectations[device_type].model().has_modifier(test_file, test_expectations.DUMPJSCONSOLELOGINSTDERR)

    def _multiply_test_inputs(self, test_inputs, repeat_each, iterations):
        if repeat_each == 1:
            per_iteration = list(test_inputs)[:]
        else:
            per_iteration = []
            for test_input in test_inputs:
                per_iteration.extend([test_input] * repeat_each)

        return per_iteration * iterations

    def _update_worker_count(self, test_inputs):
        new_test_inputs = self._multiply_test_inputs(test_inputs, self._options.repeat_each, self._options.iterations)
        worker_count = self._runner.get_worker_count(new_test_inputs, int(self._options.child_processes))
        self._options.child_processes = worker_count

    def _set_up_run(self, test_inputs, device_type):
        # This must be started before we check the system dependencies,
        # since the helper may do things to make the setup correct.
        self._printer.write_update("Starting helper ...")
        if not self._port.start_helper(pixel_tests=self._options.pixel_tests, prefer_integrated_gpu=self._options.prefer_integrated_gpu):
            return False

        self._update_worker_count(test_inputs)
        self._port.reset_preferences()

        # Check that the system dependencies (themes, fonts, ...) are correct.
        if not self._options.nocheck_sys_deps:
            self._printer.write_update("Checking system dependencies ...")
            if not self._port.check_sys_deps():
                self._port.stop_helper()
                return False

        self._port.setup_test_run(device_type)
        return True

    def run(self, args):
        num_failed_uploads = 0

        device_type_list = self._port.supported_device_types()
        try:
            tests_to_run_by_device, aggregate_tests_to_skip = self._collect_tests(args, device_type_list)
        except IOError:
            # This is raised if --test-list doesn't exist
            return test_run_results.RunDetails(exit_code=-1)

        aggregate_tests_to_run = set()  # type: Set[Test]
        for v in tests_to_run_by_device.values():
            aggregate_tests_to_run.update(v)

        skipped_tests_by_path = defaultdict(set)
        for test in aggregate_tests_to_skip:
            skipped_tests_by_path[test.test_path].add(test)

        # If a test is marked skipped, but was explicitly requested, run it anyways
        if self._options.skipped != 'always':
            for arg in args:
                if arg in skipped_tests_by_path:
                    tests = skipped_tests_by_path[arg]
                    tests_to_run_by_device[device_type_list[0]].extend(tests)
                    aggregate_tests_to_run |= tests
                    aggregate_tests_to_skip -= tests
                    del skipped_tests_by_path[arg]

        aggregate_tests = aggregate_tests_to_run | aggregate_tests_to_skip

        self._printer.print_found(len(aggregate_tests),
                                  len(aggregate_tests_to_run),
                                  self._options.repeat_each,
                                  self._options.iterations)
        start_time = time.time()

        # Check to see if all tests we are running are skipped.
        if aggregate_tests == aggregate_tests_to_skip:
            # XXX: this is currently identical to the follow if, which likely isn't intended
            _log.error("All tests skipped.")
            return test_run_results.RunDetails(exit_code=0, skipped_all_tests=True)

        # Check to make sure we have no tests to run that are not skipped.
        if not aggregate_tests_to_run:
            _log.critical('No tests to run.')
            return test_run_results.RunDetails(exit_code=-1)

        self._printer.write_update("Checking build ...")
        if not self._port.check_build():
            _log.error("Build check failed")
            return test_run_results.RunDetails(exit_code=-1)

        if self._options.clobber_old_results:
            self._clobber_old_results()

        # Create the output directory if it doesn't already exist.
        self._port.host.filesystem.maybe_make_directory(self._results_directory)

        needs_http = any(test.needs_http_server for tests in itervalues(tests_to_run_by_device) for test in tests)
        needs_web_platform_test_server = any(test.needs_wpt_server for tests in itervalues(tests_to_run_by_device) for test in tests)
        needs_websockets = any(test.needs_websocket_server for tests in itervalues(tests_to_run_by_device) for test in tests)
        self._runner = LayoutTestRunner(self._options, self._port, self._printer, self._results_directory,
                                        needs_http=needs_http, needs_web_platform_test_server=needs_web_platform_test_server, needs_websockets=needs_websockets)

        initial_results = None
        retry_results = None
        enabled_pixel_tests_in_retry = False

        max_child_processes_for_run = 1
        child_processes_option_value = self._options.child_processes
        uploads = []

        for device_type in device_type_list:
            self._options.child_processes = min(self._port.max_child_processes(device_type=device_type), int(child_processes_option_value or self._port.default_child_processes(device_type=device_type)))

            _log.info('')
            if not self._options.child_processes:
                _log.info('Skipping {} because {} is not available'.format(pluralize(len(tests_to_run_by_device[device_type]), 'test'), str(device_type)))
                _log.info('')
                continue

            max_child_processes_for_run = max(self._options.child_processes, max_child_processes_for_run)

            self._printer.print_baseline_search_path(device_type=device_type)

            _log.info(u'Running {}{}'.format(pluralize(len(tests_to_run_by_device[device_type]), 'test'), u' for {}'.format(device_type) if device_type else ''))
            _log.info('')
            start_time_for_device = time.time()
            if not tests_to_run_by_device[device_type]:
                continue

            test_inputs = [self._test_input_for_file(test, device_type=device_type)
                           for test in tests_to_run_by_device[device_type]]

            if not self._set_up_run(test_inputs, device_type=device_type):
                return test_run_results.RunDetails(exit_code=-1)

            configuration = self._port.configuration_for_upload(self._port.target_host(0))
            if not configuration.get('flavor', None):  # The --result-report-flavor argument should override wk1/wk2
                configuration['flavor'] = 'wk2' if self._options.webkit_test_runner else 'wk1'
            temp_initial_results, temp_retry_results, temp_enabled_pixel_tests_in_retry = self._run_test_subset(test_inputs, device_type=device_type)

            skipped_results = TestRunResults(self._expectations[device_type], len(aggregate_tests_to_skip))
            for skipped_test in set(aggregate_tests_to_skip):
                skipped_result = test_results.TestResult(skipped_test.test_path)
                skipped_result.type = test_expectations.SKIP
                skipped_results.add(skipped_result, expected=True)
            temp_initial_results = temp_initial_results.merge(skipped_results)

            if self._options.report_urls:
                self._printer.writeln('\n')
                self._printer.write_update('Preparing upload data ...')

                upload = Upload(
                    suite=self._options.suite or 'layout-tests',
                    configuration=configuration,
                    details=Upload.create_details(options=self._options),
                    commits=self._port.commits_for_upload(),
                    timestamp=start_time,
                    run_stats=Upload.create_run_stats(
                        start_time=start_time_for_device,
                        end_time=time.time(),
                        tests_skipped=temp_initial_results.remaining + temp_initial_results.expected_skips,
                    ),
                    results=self._results_to_upload_json_trie(self._expectations[device_type], temp_initial_results),
                )
                for hostname in self._options.report_urls:
                    self._printer.write_update('Uploading to {} ...'.format(hostname))
                    if not upload.upload(hostname, log_line_func=self._printer.writeln):
                        num_failed_uploads += 1
                    else:
                        uploads.append(upload)
                self._printer.writeln('Uploads completed!')

            initial_results = initial_results.merge(temp_initial_results) if initial_results else temp_initial_results
            retry_results = retry_results.merge(temp_retry_results) if retry_results else temp_retry_results
            enabled_pixel_tests_in_retry |= temp_enabled_pixel_tests_in_retry

            if (initial_results and (initial_results.interrupted or initial_results.keyboard_interrupted)) or \
                    (retry_results and (retry_results.interrupted or retry_results.keyboard_interrupted)):
                break

        # Used for final logging, max_child_processes_for_run is most relevant here.
        self._options.child_processes = max_child_processes_for_run

        self._runner.stop_servers()

        end_time = time.time()
        result = self._end_test_run(start_time, end_time, initial_results, retry_results, enabled_pixel_tests_in_retry)

        if self._options.report_urls and uploads:
            self._printer.writeln('\n')
            self._printer.write_update('Preparing to upload test archive ...')

            with self._filesystem.mkdtemp() as temp:
                archive = self._filesystem.join(temp, 'test-archive')
                shutil.make_archive(archive, 'zip', self._results_directory)

                for upload in uploads:
                    for hostname in self._options.report_urls:
                        self._printer.write_update('Uploading archive to {} ...'.format(hostname))
                        if not upload.upload_archive(hostname, self._filesystem.open_binary_file_for_reading(archive + '.zip'), log_line_func=self._printer.writeln):
                            num_failed_uploads += 1

        if num_failed_uploads:
            result.exit_code = -1
        return result

    def _run_test_subset(self,
                         test_inputs,  # type: List[TestInput]
                         device_type,  # type: Optional[DeviceType]
                         ):
        try:
            enabled_pixel_tests_in_retry = False
            initial_results = self._run_tests(test_inputs, self._options.repeat_each, self._options.iterations, int(self._options.child_processes), retrying=False, device_type=device_type)

            tests_to_retry = self._tests_to_retry(initial_results, include_crashes=self._port.should_retry_crashes())
            # Don't retry failures when interrupted by user or failures limit exception.
            retry_failures = self._options.retry_failures and not (initial_results.interrupted or initial_results.keyboard_interrupted)
            if retry_failures and tests_to_retry:
                enabled_pixel_tests_in_retry = self._force_pixel_tests_if_needed()
                if enabled_pixel_tests_in_retry:
                    retry_test_inputs = [self._test_input_for_file(test_input.test, device_type=device_type)
                                         for test_input in test_inputs
                                         if test_input.test.test_path in tests_to_retry]
                else:
                    retry_test_inputs = [test_input
                                         for test_input in test_inputs
                                         if test_input.test.test_path in tests_to_retry]

                _log.info('')
                _log.info("Retrying %s ..." % pluralize(len(tests_to_retry), "unexpected failure"))
                _log.info('')
                retry_results = self._run_tests(retry_test_inputs,
                                                repeat_each=1,
                                                iterations=1,
                                                num_workers=1,
                                                retrying=True,
                                                device_type=device_type)

                if enabled_pixel_tests_in_retry:
                    self._options.pixel_tests = False
            else:
                retry_results = None
        finally:
            self._clean_up_run()

        return (initial_results, retry_results, enabled_pixel_tests_in_retry)

    def _end_test_run(self, start_time, end_time, initial_results, retry_results, enabled_pixel_tests_in_retry):
        if initial_results is None:
            _log.error('No results generated')
            return test_run_results.RunDetails(exit_code=-1)

        # Some crash logs can take a long time to be written out so look
        # for new logs after the test run finishes.
        _log.debug("looking for new crash logs")
        self._look_for_new_crash_logs(initial_results, start_time)
        if retry_results:
            self._look_for_new_crash_logs(retry_results, start_time)

        _log.debug("summarizing results")
        summarized_results = test_run_results.summarize_results(self._port, self._expectations, initial_results, retry_results, enabled_pixel_tests_in_retry)
        self._printer.print_results(end_time - start_time, initial_results, summarized_results)

        exit_code = -1
        if not self._options.dry_run:
            self._port.print_leaks_summary()
            self._output_perf_metrics(end_time - start_time, initial_results)
            self._save_json_files(summarized_results, initial_results)

            results_path = self._filesystem.join(self._results_directory, "results.html")
            self._copy_results_html_file(results_path)
            if initial_results.keyboard_interrupted:
                exit_code = INTERRUPTED_EXIT_STATUS
            else:
                if self._options.show_results and (initial_results.unexpected_results_by_name or
                    (self._options.full_results_html and initial_results.total_failures)):
                    self._port.show_results_html_file(results_path)
                exit_code = self._port.exit_code_from_summarized_results(summarized_results)
        return test_run_results.RunDetails(exit_code, summarized_results, initial_results, retry_results, enabled_pixel_tests_in_retry)

    def _run_tests(self,
                   test_inputs,  # type: List[TestInput]
                   repeat_each,  # type: int
                   iterations,  # type: int
                   num_workers,  # type: int
                   retrying,  # type: bool
                   device_type,  # type: Optional[DeviceType]
                   ):
        new_test_inputs = self._multiply_test_inputs(test_inputs, repeat_each, iterations)

        assert self._runner is not None
        return self._runner.run_tests(self._expectations[device_type], new_test_inputs, num_workers, retrying, device_type)

    def _clean_up_run(self):
        _log.debug("Flushing stdout")
        sys.stdout.flush()
        _log.debug("Flushing stderr")
        sys.stderr.flush()
        _log.debug("Stopping helper")
        self._port.stop_helper()
        _log.debug("Cleaning up port")
        self._port.clean_up_test_run()

    def _force_pixel_tests_if_needed(self):
        if self._options.pixel_tests:
            return False

        _log.debug("Restarting helper")
        self._options.pixel_tests = True
        return self._port.start_helper(prefer_integrated_gpu=self._options.prefer_integrated_gpu)

    def _look_for_new_crash_logs(self, run_results, start_time):
        """Since crash logs can take a long time to be written out if the system is
           under stress do a second pass at the end of the test run.

           run_results: the results of the test run
           start_time: time the tests started at.  We're looking for crash
               logs after that time.
        """
        crashed_processes = []
        for test, result in run_results.unexpected_results_by_name.items():
            if (result.type != test_expectations.CRASH):
                continue
            for failure in result.failures:
                if not isinstance(failure, test_failures.FailureCrash):
                    continue
                crashed_processes.append([test, failure.process_name, failure.pid])

        sample_files = self._port.look_for_new_samples(crashed_processes, start_time)
        if sample_files:
            for test, sample_file in sample_files.items():
                writer = TestResultWriter(self._port._filesystem, self._port, self._port.results_directory(), test)
                writer.copy_sample_file(sample_file)

        crash_logs = self._port.look_for_new_crash_logs(crashed_processes, start_time)
        if crash_logs:
            for test, crash_log in crash_logs.items():
                writer = TestResultWriter(self._port._filesystem, self._port, self._port.results_directory(), test)
                writer.write_crash_log(crash_log)

                # Check if this crashing 'test' is already in list of crashed_processes, if not add it to the run_results
                if not any(process[0] == test for process in crashed_processes):
                    result = test_results.TestResult(test)
                    result.type = test_expectations.CRASH
                    result.is_other_crash = True
                    run_results.add(result, expected=False)
                    _log.debug("Adding results for other crash: " + str(test))

    def _clobber_old_results(self):
        self._printer.write_update("Deleting results directory {}".format(self._results_directory))
        if self._filesystem.isdir(self._results_directory):
            self._filesystem.rmtree(self._results_directory)

    def _tests_to_retry(self, run_results, include_crashes):
        return [result.test_name for result in run_results.unexpected_results_by_name.values() if
                   ((result.type != test_expectations.PASS) and
                    (result.type != test_expectations.MISSING) and
                    (result.type != test_expectations.CRASH or include_crashes))]

    def _output_perf_metrics(self, run_time, initial_results):
        perf_metrics_json = json_results_generator.perf_metrics_for_test(run_time, initial_results.results_by_name.values())
        perf_metrics_path = self._filesystem.join(self._results_directory, "layout_test_perf_metrics.json")
        self._filesystem.write_text_file(perf_metrics_path, json.dumps(perf_metrics_json))

    def _results_to_upload_json_trie(self, expectations, results):
        FAILURE_TO_TEXT = {
            test_expectations.PASS: Upload.Expectations.PASS,
            test_expectations.CRASH: Upload.Expectations.CRASH,
            test_expectations.TIMEOUT: Upload.Expectations.TIMEOUT,
            test_expectations.IMAGE: Upload.Expectations.IMAGE,
            test_expectations.TEXT: Upload.Expectations.TEXT,
            test_expectations.AUDIO: Upload.Expectations.AUDIO,
            test_expectations.MISSING: Upload.Expectations.WARNING,
            test_expectations.IMAGE_PLUS_TEXT: ' '.join([Upload.Expectations.IMAGE, Upload.Expectations.TEXT]),
        }

        results_trie = {}
        for result in itervalues(results.results_by_name):
            if result.type == test_expectations.SKIP:
                continue

            expected = expectations.filtered_expectations_for_test(
                result.test_name,
                self._options.pixel_tests or bool(result.reftest_type),
                self._options.world_leaks,
            )
            if expected == {test_expectations.PASS}:
                expected = None
            else:
                expected = ' '.join([FAILURE_TO_TEXT.get(e, Upload.Expectations.FAIL) for e in expected])

            json_results_generator.add_path_to_trie(
                result.test_name,
                Upload.create_test_result(
                    expected=expected,
                    actual=FAILURE_TO_TEXT.get(result.type, Upload.Expectations.FAIL) if result.type else None,
                    time=int(result.test_run_time * 1000),
                ), results_trie)
        return results_trie

    def _save_json_files(self, summarized_results, initial_results):
        """Writes the results of the test run as JSON files into the results
        dir and upload the files to the appengine server.

        Args:
          summarized_results: dict of results
          initial_results: full summary object
        """
        _log.debug("Writing JSON files in %s." % self._results_directory)

        # FIXME: Upload stats.json to the server and delete times_ms.
        times_trie = json_results_generator.test_timings_trie(self._port, initial_results.results_by_name.values())
        times_json_path = self._filesystem.join(self._results_directory, "times_ms.json")
        json_results_generator.write_json(self._filesystem, times_trie, times_json_path)

        stats_trie = self._stats_trie(initial_results)
        stats_path = self._filesystem.join(self._results_directory, "stats.json")
        self._filesystem.write_text_file(stats_path, json.dumps(stats_trie))

        full_results_path = self._filesystem.join(self._results_directory, "full_results.json")
        # We write full_results.json out as jsonp because we need to load it from a file url and Chromium doesn't allow that.
        json_results_generator.write_json(self._filesystem, summarized_results, full_results_path, callback="ADD_RESULTS")

        generator = json_layout_results_generator.JSONLayoutResultsGenerator(
            self._port, self._results_directory,
            self._expectations, initial_results,
            "layout-tests")

        if generator.generate_json_output():
            _log.debug("Finished writing JSON file for the test results server.")
        else:
            _log.debug("Failed to generate JSON file for the test results server.")
            return

        incremental_results_path = self._filesystem.join(self._results_directory, "incremental_results.json")

        # Remove these files from the results directory so they don't take up too much space on the buildbot.
        # The tools use the version we uploaded to the results server anyway.
        self._filesystem.remove(times_json_path)
        self._filesystem.remove(incremental_results_path)

    def _copy_results_html_file(self, destination_path):
        base_dir = self._port.path_from_webkit_base('LayoutTests', 'fast', 'harness')
        results_file = self._filesystem.join(base_dir, 'results.html')
        # Note that the results.html template file won't exist when we're using a MockFileSystem during unit tests,
        # so make sure it exists before we try to copy it.
        if self._filesystem.exists(results_file):
            self._filesystem.copyfile(results_file, destination_path)

    def _stats_trie(self, initial_results):
        def _worker_number(worker_name):
            return int(worker_name.split('/')[1]) if worker_name else -1

        stats = {}
        for result in initial_results.results_by_name.values():
            if result.type != test_expectations.SKIP:
                stats[result.test_name] = {'results': (_worker_number(result.worker_name), result.test_number, result.pid, int(result.test_run_time * 1000), int(result.total_run_time * 1000))}
        stats_trie = {}
        for name, value in iteritems(stats):
            json_results_generator.add_path_to_trie(name, value, stats_trie)
        return stats_trie

    def _print_expectation_line_for_test(self, format_string, test, device_type):
        test_path = test.test_path
        line = self._expectations[device_type].model().get_expectation_line(test_path)
        print(format_string.format(test_path,
                                   line.expected_behavior,
                                   self._expectations[device_type].readable_filename_and_line_number(line),
                                   line.original_string or ''))

    def _print_expectations_for_subset(self, device_type, test_col_width, tests_to_run, tests_to_skip=None):
        format_string = '{{:{width}}} {{}} {{}} {{}}'.format(width=test_col_width)
        if tests_to_skip:
            print('')
            print('Tests to skip ({})'.format(len(tests_to_skip)))
            for test in sorted(tests_to_skip):
                self._print_expectation_line_for_test(format_string, test, device_type=device_type)

        print('')
        print('Tests to run{} ({})'.format(' for ' + str(device_type) if device_type else '', len(tests_to_run)))
        for test in sorted(tests_to_run):
            self._print_expectation_line_for_test(format_string, test, device_type=device_type)

    def print_expectations(self, args):
        device_type_list = self._port.supported_device_types()

        try:
            tests_to_run_by_device, aggregate_tests_to_skip = self._collect_tests(args, device_type_list)
        except IOError:
            # This is raised if --test-list doesn't exist
            return -1

        aggregate_tests_to_run = set()
        for v in tests_to_run_by_device.values():
            aggregate_tests_to_run.update(v)
        aggregate_tests = aggregate_tests_to_run | aggregate_tests_to_skip

        self._printer.print_found(len(aggregate_tests), len(aggregate_tests_to_run), self._options.repeat_each, self._options.iterations)
        test_col_width = max(len(test.test_path) for test in aggregate_tests) + 1

        self._print_expectations_for_subset(device_type_list[0], test_col_width, tests_to_run_by_device[device_type_list[0]], aggregate_tests_to_skip)

        for device_type in device_type_list[1:]:
            self._print_expectations_for_subset(device_type, test_col_width, tests_to_run_by_device[device_type])

        return 0

    def print_summary(self, args):
        device_type_list = self._port.supported_device_types()
        test_stats = {}

        try:
            self._collect_tests(args, device_type_list)
        except IOError:
            # This is raised if --test-list doesn't exist
            return test_run_results.RunDetails(exit_code=-1)

        for device_type, expectations in self._expectations.items():
            test_stats[device_type] = {'__root__': {'count': 0, 'skip': 0, 'pass': 0, 'flaky': 0, 'fail': 0, 'has_tests': False}}
            device_test_stats = test_stats[device_type]

            model = expectations.model()
            tests_skipped = model.get_tests_with_result_type(test_expectations.SKIP)
            tests_passing = model.get_tests_with_result_type(test_expectations.PASS)
            tests_flaky = model.get_tests_with_result_type(test_expectations.FLAKY)
            tests_misc_fail = model.get_tests_with_result_type(test_expectations.FAIL)

            def _increment_stat(dirname, test_name, test_in_directory):
                if dirname in device_test_stats:
                    device_test_stats[dirname]['count'] += 1
                else:
                    device_test_stats[dirname] = {'count': 1, 'skip': 0, 'pass': 0, 'flaky': 0, 'fail': 0, 'has_tests': False}

                if test_name in tests_skipped:
                    device_test_stats[dirname]['skip'] += 1
                if test_name in tests_passing:
                    device_test_stats[dirname]['pass'] += 1
                if test_name in tests_flaky:
                    device_test_stats[dirname]['flaky'] += 1
                if test_name in tests_misc_fail:
                    device_test_stats[dirname]['fail'] += 1
                device_test_stats[dirname]['has_tests'] = device_test_stats[dirname]['has_tests'] or test_in_directory

            for test_name in expectations._full_test_list:
                path = test_name
                test_in_directory = True
                while path != '':
                    path = os.path.dirname(path)
                    _increment_stat(path or '__root__', test_name, test_in_directory)
                    test_in_directory = False

            print('')
            print('Summary of test expectations {}in layout test directories:'.format(u'for {} '.format(device_type) if device_type else ''))
            root = device_test_stats['__root__']
            print('    {} total tests'.format(root['count']))
            print('    {} pass'.format(root['pass']))
            print('    {} are skipped'.format(root['skip']))
            print('    {} are flaky'.format(root['flaky']))
            print('    {} fail for other reasons'.format(root['fail']))

            print('')
            print('Per directory results:')
            print('(* means there are no tests in that specific directory)')
            print('')
            row_format = u'    {0:50s}{1:>8d}{2:>8d}{3:>8d}{4:>8d}{5:>8d}'
            srow_format = row_format.replace('d', 's')
            print(srow_format.format('DIRECTORY', 'TOTAL', 'PASS', 'SKIP', 'FLAKY', 'FAIL'))
            print(srow_format.format('---------', '-----', '----', '----', '-----', '----'))

            def _should_include_dir_in_report(dirname):
                num_dirs = dirname.count('/')
                if num_dirs > 0:
                    if num_dirs > 1:
                        if num_dirs > 2:
                            if num_dirs > 3:
                                return False
                            elif not re.match(r'^(imported/w3c/web-platform-tests)/', dirname):
                                return False
                        elif not re.match(r'(imported|http|platform)/', dirname):
                            return False
                    elif not re.match(r'^(fast|platform|imported|http)/', dirname):
                        return False
                return True

            for dirname in sorted(device_test_stats.keys()):
                if not _should_include_dir_in_report(dirname):
                    continue

                truncated_dirname = re.sub(r'^.*(.{47})$', '...\\g<1>', dirname if device_test_stats[dirname]['has_tests'] else '{}*'.format(dirname))
                count = device_test_stats[dirname]['count']
                passing = device_test_stats[dirname]['pass']
                skip = device_test_stats[dirname]['skip']
                flaky = device_test_stats[dirname]['flaky']
                fail = device_test_stats[dirname]['fail']
                if passing == count:
                    # Don't print this line if an ancestor directory is all pass also
                    ancestor_dirname = os.path.dirname(dirname)
                    while ancestor_dirname and ancestor_dirname not in device_test_stats:
                        ancestor_dirname = os.path.dirname(ancestor_dirname)
                    if ancestor_dirname and device_test_stats[ancestor_dirname]['pass'] == device_test_stats[ancestor_dirname]['count']:
                        continue
                    print(srow_format.format(truncated_dirname, str(count), u"██ PASS", u' ███████', u'████████', u'████████'))
                    continue
                elif skip == count:
                    # Don't print this line if an ancestor directory is all skip also
                    ancestor_dirname = os.path.dirname(dirname)
                    while ancestor_dirname and ancestor_dirname not in device_test_stats:
                        ancestor_dirname = os.path.dirname(ancestor_dirname)
                    if ancestor_dirname and device_test_stats[ancestor_dirname]['skip'] == device_test_stats[ancestor_dirname]['count']:
                        continue
                    print(srow_format.format(truncated_dirname, str(count), u'░░░░░░░', u"░░░ SKIP", u' ░░░░░░░', u'░░░░░░░░'))
                    continue
                print(row_format.format(truncated_dirname, count, passing, skip, flaky, fail))

        with open('layout_tests.csv', 'w') as csvfile:
            writer = csv.writer(csvfile, delimiter=',', quoting=csv.QUOTE_MINIMAL)

            header_row_1 = [''] * 2
            header_row_2 = ["Directory", "Total"]
            for device_type in sorted(device_type_list):
                header_row_1.append('{}{}'.format(self._port.name(), ', {}'.format(device_type) if device_type else ''))
                header_row_1.extend([''] * 3)
                header_row_2.extend(['Pass', 'Skip', 'Flaky', 'Fail'])
            writer.writerow(header_row_1)
            writer.writerow(header_row_2)

            a_device_test_stat = test_stats[device_type_list[0]]
            for dirname in sorted(a_device_test_stat.keys()):
                if not _should_include_dir_in_report(dirname):
                    continue
                row = [dirname, a_device_test_stat[dirname]['count']]
                for device_type in sorted(device_type_list):
                    stats = test_stats[device_type][dirname]
                    row.extend([stats['pass'], stats['skip'], stats['flaky'], stats['fail']])
                writer.writerow(row)

        return 0
