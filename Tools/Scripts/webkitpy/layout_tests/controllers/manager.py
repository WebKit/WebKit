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

import json
import logging
import random
import sys
import time
from collections import defaultdict, OrderedDict

from webkitpy.common.checkout.scm.detection import SCMDetector
from webkitpy.common.net.file_uploader import FileUploader
from webkitpy.layout_tests.controllers.layout_test_finder import LayoutTestFinder
from webkitpy.layout_tests.controllers.layout_test_runner import LayoutTestRunner
from webkitpy.layout_tests.controllers.test_result_writer import TestResultWriter
from webkitpy.layout_tests.layout_package import json_layout_results_generator
from webkitpy.layout_tests.layout_package import json_results_generator
from webkitpy.layout_tests.models import test_expectations
from webkitpy.layout_tests.models import test_failures
from webkitpy.layout_tests.models import test_results
from webkitpy.layout_tests.models import test_run_results
from webkitpy.layout_tests.models.test_input import TestInput
from webkitpy.layout_tests.models.test_run_results import INTERRUPTED_EXIT_STATUS
from webkitpy.tool.grammar import pluralize
from webkitpy.results.upload import Upload
from webkitpy.xcode.device_type import DeviceType

_log = logging.getLogger(__name__)

TestExpectations = test_expectations.TestExpectations


class Manager(object):
    """A class for managing running a series of tests on a series of layout
    test files."""

    def __init__(self, port, options, printer):
        """Initialize test runner data structures.

        Args:
          port: an object implementing port-specific
          options: a dictionary of command line options
          printer: a Printer object to record updates to.
        """
        self._port = port
        self._filesystem = port.host.filesystem
        self._options = options
        self._printer = printer
        self._expectations = OrderedDict()
        self.HTTP_SUBDIR = 'http' + port.TEST_PATH_SEPARATOR + 'test'
        self.WEBSOCKET_SUBDIR = 'websocket' + port.TEST_PATH_SEPARATOR
        self.web_platform_test_subdir = self._port.web_platform_test_server_doc_root()
        self.webkit_specific_web_platform_test_subdir = 'http' + port.TEST_PATH_SEPARATOR + 'wpt' + port.TEST_PATH_SEPARATOR
        self.LAYOUT_TESTS_DIRECTORY = 'LayoutTests'
        self._results_directory = self._port.results_directory()
        self._finder = LayoutTestFinder(self._port, self._options)
        self._runner = None

        test_options_json_path = self._port.path_from_webkit_base(self.LAYOUT_TESTS_DIRECTORY, "tests-options.json")
        self._tests_options = json.loads(self._filesystem.read_text_file(test_options_json_path)) if self._filesystem.exists(test_options_json_path) else {}

    def _collect_tests(self, args, device_type=None):
        return self._finder.find_tests(self._options, args, device_type=device_type)

    def _is_http_test(self, test):
        return self.HTTP_SUBDIR in test or self._is_websocket_test(test) or self._needs_web_platform_test(test)

    def _is_websocket_test(self, test):
        return self.WEBSOCKET_SUBDIR in test

    def _needs_web_platform_test(self, test):
        return self.web_platform_test_subdir in test or self.webkit_specific_web_platform_test_subdir in test

    def _http_tests(self, test_names):
        return set(test for test in test_names if self._is_http_test(test))

    def _prepare_lists(self, paths, test_names, device_type=None):
        tests_to_skip = self._finder.skip_tests(paths, test_names, self._expectations[device_type], self._http_tests(test_names))
        tests_to_run = [test for test in test_names if test not in tests_to_skip]

        # Create a sorted list of test files so the subset chunk,
        # if used, contains alphabetically consecutive tests.
        if self._options.order == 'natural':
            tests_to_run.sort(key=self._port.test_key)
        elif self._options.order == 'random':
            random.shuffle(tests_to_run)

        tests_to_run, tests_in_other_chunks = self._finder.split_into_chunks(tests_to_run)
        self._expectations[device_type].add_skipped_tests(tests_in_other_chunks)
        tests_to_skip.update(tests_in_other_chunks)

        return tests_to_run, tests_to_skip

    def _test_input_for_file(self, test_file, device_type=None):
        return TestInput(test_file,
            self._options.slow_time_out_ms if self._test_is_slow(test_file, device_type=device_type) else self._options.time_out_ms,
            self._is_http_test(test_file),
            should_dump_jsconsolelog_in_stderr=self._test_should_dump_jsconsolelog_in_stderr(test_file, device_type=device_type))

    def _test_is_slow(self, test_file, device_type=None):
        if self._expectations[device_type].model().has_modifier(test_file, test_expectations.SLOW):
            return True
        return "slow" in self._tests_options.get(test_file, [])

    def _test_should_dump_jsconsolelog_in_stderr(self, test_file, device_type=None):
        return self._expectations[device_type].model().has_modifier(test_file, test_expectations.DUMPJSCONSOLELOGINSTDERR)

    def needs_servers(self, test_names):
        return any(self._is_http_test(test_name) for test_name in test_names) and self._options.http

    def _get_test_inputs(self, tests_to_run, repeat_each, iterations, device_type=None):
        test_inputs = []
        for _ in xrange(iterations):
            for test in tests_to_run:
                for _ in xrange(repeat_each):
                    test_inputs.append(self._test_input_for_file(test, device_type=device_type))
        return test_inputs

    def _update_worker_count(self, test_names, device_type=None):
        test_inputs = self._get_test_inputs(test_names, self._options.repeat_each, self._options.iterations, device_type=device_type)
        worker_count = self._runner.get_worker_count(test_inputs, int(self._options.child_processes))
        self._options.child_processes = worker_count

    def _set_up_run(self, test_names, device_type=None):
        # This must be started before we check the system dependencies,
        # since the helper may do things to make the setup correct.
        self._printer.write_update("Starting helper ...")
        if not self._port.start_helper(self._options.pixel_tests):
            return False

        self._update_worker_count(test_names, device_type=device_type)
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
        total_tests = set()
        aggregate_test_names = set()
        aggregate_tests = set()
        tests_to_run_by_device = {}

        device_type_list = self._port.supported_device_types()
        for device_type in device_type_list:
            """Run the tests and return a RunDetails object with the results."""
            for_device_type = u'for {} '.format(device_type) if device_type else ''
            self._printer.write_update(u'Collecting tests {}...'.format(for_device_type))
            try:
                paths, test_names = self._collect_tests(args, device_type=device_type)
            except IOError:
                # This is raised if --test-list doesn't exist
                return test_run_results.RunDetails(exit_code=-1)

            self._printer.write_update(u'Parsing expectations {}...'.format(for_device_type))
            self._expectations[device_type] = test_expectations.TestExpectations(self._port, test_names, force_expectations_pass=self._options.force, device_type=device_type)
            self._expectations[device_type].parse_all_expectations()

            aggregate_test_names.update(test_names)
            tests_to_run, tests_to_skip = self._prepare_lists(paths, test_names, device_type=device_type)

            total_tests.update(tests_to_run)
            total_tests.update(tests_to_skip)

            tests_to_run_by_device[device_type] = [test for test in tests_to_run if test not in aggregate_tests]
            aggregate_tests.update(tests_to_run)

        # If a test is marked skipped, but was explicitly requested, run it anyways
        if self._options.skipped != 'always':
            for arg in args:
                if arg in total_tests and arg not in aggregate_tests:
                    tests_to_run_by_device[device_type_list[0]].append(arg)
                    aggregate_tests.add(arg)

        tests_to_skip = total_tests - aggregate_tests
        self._printer.print_found(len(aggregate_test_names), len(aggregate_tests), self._options.repeat_each, self._options.iterations)
        start_time = time.time()

        # Check to make sure we're not skipping every test.
        if not sum([len(tests) for tests in tests_to_run_by_device.itervalues()]):
            _log.critical('No tests to run.')
            return test_run_results.RunDetails(exit_code=-1)

        needs_http = any((self._is_http_test(test) and not self._needs_web_platform_test(test)) for tests in tests_to_run_by_device.itervalues() for test in tests)
        needs_web_platform_test_server = any(self._needs_web_platform_test(test) for tests in tests_to_run_by_device.itervalues() for test in tests)
        needs_websockets = any(self._is_websocket_test(test) for tests in tests_to_run_by_device.itervalues() for test in tests)
        self._runner = LayoutTestRunner(self._options, self._port, self._printer, self._results_directory, self._test_is_slow,
                                        needs_http=needs_http, needs_web_platform_test_server=needs_web_platform_test_server, needs_websockets=needs_websockets)

        self._printer.write_update("Checking build ...")
        if not self._port.check_build():
            _log.error("Build check failed")
            return test_run_results.RunDetails(exit_code=-1)

        if self._options.clobber_old_results:
            self._clobber_old_results()

        # Create the output directory if it doesn't already exist.
        self._port.host.filesystem.maybe_make_directory(self._results_directory)

        initial_results = None
        retry_results = None
        enabled_pixel_tests_in_retry = False

        max_child_processes_for_run = 1
        child_processes_option_value = self._options.child_processes

        for device_type in device_type_list:
            self._runner._test_is_slow = lambda test_file: self._test_is_slow(test_file, device_type=device_type)
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
            if not self._set_up_run(tests_to_run_by_device[device_type], device_type=device_type):
                return test_run_results.RunDetails(exit_code=-1)

            configuration = self._port.configuration_for_upload(self._port.target_host(0))
            if not configuration.get('flavor', None):  # The --result-report-flavor argument should override wk1/wk2
                configuration['flavor'] = 'wk2' if self._options.webkit_test_runner else 'wk1'
            temp_initial_results, temp_retry_results, temp_enabled_pixel_tests_in_retry = self._run_test_subset(tests_to_run_by_device[device_type], tests_to_skip, device_type=device_type)

            if self._options.report_urls:
                self._printer.writeln('\n')
                self._printer.write_update('Preparing upload data ...')

                upload = Upload(
                    suite='layout-tests',
                    configuration=configuration,
                    details=Upload.create_details(options=self._options),
                    commits=self._port.commits_for_upload(),
                    run_stats=Upload.create_run_stats(
                        start_time=start_time_for_device,
                        end_time=time.time(),
                        tests_skipped=temp_initial_results.remaining + temp_initial_results.expected_skips,
                    ),
                    results=self._results_to_upload_json_trie(self._expectations[device_type], temp_initial_results),
                )
                for url in self._options.report_urls:
                    self._printer.write_update('Uploading to {} ...'.format(url))
                    if not upload.upload(url, log_line_func=self._printer.writeln):
                        num_failed_uploads += 1
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
        if num_failed_uploads:
            result.exit_code = -1
        return result

    def _run_test_subset(self, tests_to_run, tests_to_skip, device_type=None):
        try:
            enabled_pixel_tests_in_retry = False
            initial_results = self._run_tests(tests_to_run, tests_to_skip, self._options.repeat_each, self._options.iterations, int(self._options.child_processes), retrying=False, device_type=device_type)

            tests_to_retry = self._tests_to_retry(initial_results, include_crashes=self._port.should_retry_crashes())
            # Don't retry failures when interrupted by user or failures limit exception.
            retry_failures = self._options.retry_failures and not (initial_results.interrupted or initial_results.keyboard_interrupted)
            if retry_failures and tests_to_retry:
                enabled_pixel_tests_in_retry = self._force_pixel_tests_if_needed()

                _log.info('')
                _log.info("Retrying %s ..." % pluralize(len(tests_to_retry), "unexpected failure"))
                _log.info('')
                retry_results = self._run_tests(tests_to_retry, tests_to_skip=set(), repeat_each=1, iterations=1, num_workers=1, retrying=True, device_type=device_type)

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
        results_including_passes = None
        if self._options.results_server_host:
            results_including_passes = test_run_results.summarize_results(self._port, self._expectations, initial_results, retry_results, enabled_pixel_tests_in_retry, include_passes=True, include_time_and_modifiers=True)
        self._printer.print_results(end_time - start_time, initial_results, summarized_results)

        exit_code = -1
        if not self._options.dry_run:
            self._port.print_leaks_summary()
            self._output_perf_metrics(end_time - start_time, initial_results)
            self._upload_json_files(summarized_results, initial_results, results_including_passes, start_time, end_time)

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

    def _run_tests(self, tests_to_run, tests_to_skip, repeat_each, iterations, num_workers, retrying, device_type=None):
        test_inputs = self._get_test_inputs(tests_to_run, repeat_each, iterations, device_type=device_type)

        return self._runner.run_tests(self._expectations[device_type], test_inputs, tests_to_skip, num_workers, retrying)

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
        self._port.stop_helper()
        self._options.pixel_tests = True
        return self._port.start_helper()

    def _look_for_new_crash_logs(self, run_results, start_time):
        """Since crash logs can take a long time to be written out if the system is
           under stress do a second pass at the end of the test run.

           run_results: the results of the test run
           start_time: time the tests started at.  We're looking for crash
               logs after that time.
        """
        crashed_processes = []
        for test, result in run_results.unexpected_results_by_name.iteritems():
            if (result.type != test_expectations.CRASH):
                continue
            for failure in result.failures:
                if not isinstance(failure, test_failures.FailureCrash):
                    continue
                crashed_processes.append([test, failure.process_name, failure.pid])

        sample_files = self._port.look_for_new_samples(crashed_processes, start_time)
        if sample_files:
            for test, sample_file in sample_files.iteritems():
                writer = TestResultWriter(self._port._filesystem, self._port, self._port.results_directory(), test)
                writer.copy_sample_file(sample_file)

        crash_logs = self._port.look_for_new_crash_logs(crashed_processes, start_time)
        if crash_logs:
            for test, crash_log in crash_logs.iteritems():
                writer = TestResultWriter(self._port._filesystem, self._port, self._port.results_directory(), test)
                writer.write_crash_log(crash_log)

                # Check if this crashing 'test' is already in list of crashed_processes, if not add it to the run_results
                if not any(process[0] == test for process in crashed_processes):
                    result = test_results.TestResult(test)
                    result.type = test_expectations.CRASH
                    result.is_other_crash = True
                    run_results.add(result, expected=False, test_is_slow=False)
                    _log.debug("Adding results for other crash: " + str(test))

    def _clobber_old_results(self):
        # Just clobber the actual test results directories since the other
        # files in the results directory are explicitly used for cross-run
        # tracking.
        self._printer.write_update("Clobbering old results in %s" %
                                   self._results_directory)
        layout_tests_dir = self._port.layout_tests_dir()
        possible_dirs = self._port.test_dirs()
        for dirname in possible_dirs:
            if self._filesystem.isdir(self._filesystem.join(layout_tests_dir, dirname)):
                self._filesystem.rmtree(self._filesystem.join(self._results_directory, dirname))

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
        for result in results.results_by_name.itervalues():
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

    def _upload_json_files(self, summarized_results, initial_results, results_including_passes=None, start_time=None, end_time=None):
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

        results_json_path = self._filesystem.join(self._results_directory, "results_including_passes.json")
        if results_including_passes:
            json_results_generator.write_json(self._filesystem, results_including_passes, results_json_path)

        generator = json_layout_results_generator.JSONLayoutResultsGenerator(
            self._port, self._options.builder_name, self._options.build_name,
            self._options.build_number, self._results_directory,
            self._expectations, initial_results,
            self._options.test_results_server,
            "layout-tests",
            self._options.master_name)

        if generator.generate_json_output():
            _log.debug("Finished writing JSON file for the test results server.")
        else:
            _log.debug("Failed to generate JSON file for the test results server.")
            return

        json_files = ["incremental_results.json", "full_results.json", "times_ms.json"]

        generator.upload_json_files(json_files)
        if results_including_passes:
            self.upload_results(results_json_path, start_time, end_time)

        incremental_results_path = self._filesystem.join(self._results_directory, "incremental_results.json")

        # Remove these files from the results directory so they don't take up too much space on the buildbot.
        # The tools use the version we uploaded to the results server anyway.
        self._filesystem.remove(times_json_path)
        self._filesystem.remove(incremental_results_path)
        if results_including_passes:
            self._filesystem.remove(results_json_path)

    def upload_results(self, results_json_path, start_time, end_time):
        if not self._options.results_server_host:
            return
        master_name = self._options.master_name
        builder_name = self._options.builder_name
        build_number = self._options.build_number
        build_slave = self._options.build_slave
        if not master_name or not builder_name or not build_number or not build_slave:
            _log.error("--results-server-host was set, but --master-name, --builder-name, --build-number, or --build-slave was not. Not uploading JSON files.")
            return

        revisions = {}
        # FIXME: This code is duplicated in PerfTestRunner._generate_results_dict
        for (name, path) in self._port.repository_paths():
            scm = SCMDetector(self._port.host.filesystem, self._port.host.executive).detect_scm_system(path) or self._port.host.scm()
            revision = scm.native_revision(path)
            revisions[name] = {'revision': revision, 'timestamp': scm.timestamp_of_native_revision(path, revision)}

        for hostname in self._options.results_server_host:
            _log.info("Uploading JSON files for master: %s builder: %s build: %s slave: %s to %s", master_name, builder_name, build_number, build_slave, hostname)

            attrs = [
                ('master', 'build.webkit.org' if master_name == 'webkit.org' else master_name),  # FIXME: Pass in build.webkit.org.
                ('builder_name', builder_name),
                ('build_number', build_number),
                ('build_slave', build_slave),
                ('revisions', json.dumps(revisions)),
                ('start_time', str(start_time)),
                ('end_time', str(end_time)),
            ]

            uploader = FileUploader("http://%s/api/report" % hostname, 360)
            try:
                response = uploader.upload_as_multipart_form_data(self._filesystem, [('results.json', results_json_path)], attrs)
                if not response:
                    _log.error("JSON upload failed; no response returned")
                    continue

                if response.code != 200:
                    _log.error("JSON upload failed, %d: '%s'" % (response.code, response.read()))
                    continue

                response_text = response.read()
                try:
                    response_json = json.loads(response_text)
                except ValueError as error:
                    _log.error("JSON upload failed; failed to parse the response: %s", response_text)
                    continue

                if response_json['status'] != 'OK':
                    _log.error("JSON upload failed, %s: %s", response_json['status'], response_text)
                    continue

                _log.info("JSON uploaded.")
            except Exception as error:
                _log.error("Upload failed: %s" % error)
                continue

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
        for name, value in stats.iteritems():
            json_results_generator.add_path_to_trie(name, value, stats_trie)
        return stats_trie

    def _print_expectation_line_for_test(self, format_string, test, device_type=None):
        line = self._expectations[device_type].model().get_expectation_line(test)
        print(format_string.format(test, line.expected_behavior, self._expectations[device_type].readable_filename_and_line_number(line), line.original_string or ''))

    def _print_expectations_for_subset(self, device_type, test_col_width, tests_to_run, tests_to_skip={}):
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
        aggregate_test_names = set()
        aggregate_tests_to_run = set()
        aggregate_tests_to_skip = set()
        tests_to_run_by_device = {}

        device_type_list = self._port.DEFAULT_DEVICE_TYPES or [self._port.DEVICE_TYPE]
        for device_type in device_type_list:
            """Run the tests and return a RunDetails object with the results."""
            for_device_type = 'for {} '.format(device_type) if device_type else ''
            self._printer.write_update('Collecting tests {}...'.format(for_device_type))
            try:
                paths, test_names = self._collect_tests(args, device_type=device_type)
            except IOError:
                # This is raised if --test-list doesn't exist
                return test_run_results.RunDetails(exit_code=-1)

            self._printer.write_update('Parsing expectations {}...'.format(for_device_type))
            self._expectations[device_type] = test_expectations.TestExpectations(self._port, test_names, force_expectations_pass=self._options.force, device_type=device_type)
            self._expectations[device_type].parse_all_expectations()

            aggregate_test_names.update(test_names)
            tests_to_run, tests_to_skip = self._prepare_lists(paths, test_names, device_type=device_type)
            aggregate_tests_to_skip.update(tests_to_skip)

            tests_to_run_by_device[device_type] = [test for test in tests_to_run if test not in aggregate_tests_to_run]
            aggregate_tests_to_run.update(tests_to_run)

        aggregate_tests_to_skip = aggregate_tests_to_skip - aggregate_tests_to_run

        self._printer.print_found(len(aggregate_test_names), len(aggregate_tests_to_run), self._options.repeat_each, self._options.iterations)
        test_col_width = len(max(aggregate_tests_to_run.union(aggregate_tests_to_skip), key=len)) + 1

        self._print_expectations_for_subset(device_type_list[0], test_col_width, tests_to_run_by_device[device_type_list[0]], aggregate_tests_to_skip)

        for device_type in device_type_list[1:]:
            self._print_expectations_for_subset(device_type, test_col_width, tests_to_run_by_device[device_type])

        return 0
