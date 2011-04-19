#!/usr/bin/env python
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
The TestRunner class runs a series of tests (TestType interface) against a set
of test files.  If a test file fails a TestType, it returns a list TestFailure
objects to the TestRunner.  The TestRunner then aggregates the TestFailures to
create a final report.
"""

from __future__ import with_statement

import copy
import errno
import logging
import math
import Queue
import random
import sys
import time

from webkitpy.layout_tests.layout_package import json_layout_results_generator
from webkitpy.layout_tests.layout_package import json_results_generator
from webkitpy.layout_tests.layout_package import printing
from webkitpy.layout_tests.layout_package import test_expectations
from webkitpy.layout_tests.layout_package import test_failures
from webkitpy.layout_tests.layout_package import test_results
from webkitpy.layout_tests.layout_package import test_results_uploader
from webkitpy.layout_tests.layout_package.result_summary import ResultSummary
from webkitpy.layout_tests.layout_package.test_input import TestInput

from webkitpy.thirdparty import simplejson
from webkitpy.tool import grammar

_log = logging.getLogger("webkitpy.layout_tests.run_webkit_tests")

# Builder base URL where we have the archived test results.
BUILDER_BASE_URL = "http://build.chromium.org/buildbot/layout_test_results/"

TestExpectationsFile = test_expectations.TestExpectationsFile


def summarize_results(port_obj, expectations, result_summary, retry_summary, test_timings, only_unexpected):
    """Summarize any unexpected results as a dict.

    FIXME: split this data structure into a separate class?

    Args:
        port_obj: interface to port-specific hooks
        expectations: test_expectations.TestExpectations object
        result_summary: summary object from initial test runs
        retry_summary: summary object from final test run of retried tests
        test_timings: a list of TestResult objects which contain test runtimes in seconds
        only_unexpected: whether to return a summary only for the unexpected results
    Returns:
        A dictionary containing a summary of the unexpected results from the
        run, with the following fields:
        'version': a version indicator (1 in this version)
        'fixable': # of fixable tests (NOW - PASS)
        'skipped': # of skipped tests (NOW & SKIPPED)
        'num_regressions': # of non-flaky failures
        'num_flaky': # of flaky failures
        'num_passes': # of unexpected passes
        'tests': a dict of tests -> {'expected': '...', 'actual': '...', 'time_ms': ...}
    """
    results = {}
    results['version'] = 1

    test_timings_map = dict((test_result.filename, test_result.test_run_time) for test_result in test_timings)

    tbe = result_summary.tests_by_expectation
    tbt = result_summary.tests_by_timeline
    results['fixable'] = len(tbt[test_expectations.NOW] -
                                tbe[test_expectations.PASS])
    results['skipped'] = len(tbt[test_expectations.NOW] &
                                tbe[test_expectations.SKIP])

    num_passes = 0
    num_flaky = 0
    num_regressions = 0
    keywords = {}
    for expecation_string, expectation_enum in TestExpectationsFile.EXPECTATIONS.iteritems():
        keywords[expectation_enum] = expecation_string.upper()

    for modifier_string, modifier_enum in TestExpectationsFile.MODIFIERS.iteritems():
        keywords[modifier_enum] = modifier_string.upper()

    tests = {}
    original_results = result_summary.unexpected_results if only_unexpected else result_summary.results

    for filename, result in original_results.iteritems():
        # Note that if a test crashed in the original run, we ignore
        # whether or not it crashed when we retried it (if we retried it),
        # and always consider the result not flaky.
        test = port_obj.relative_test_filename(filename)
        expected = expectations.get_expectations_string(filename)
        result_type = result.type
        actual = [keywords[result_type]]

        if result_type == test_expectations.PASS:
            num_passes += 1
        elif result_type == test_expectations.CRASH:
            num_regressions += 1
        elif filename in result_summary.unexpected_results:
            if filename not in retry_summary.unexpected_results:
                actual.extend(expectations.get_expectations_string(filename).split(" "))
                num_flaky += 1
            else:
                retry_result_type = retry_summary.unexpected_results[filename].type
                if result_type != retry_result_type:
                    actual.append(keywords[retry_result_type])
                    num_flaky += 1
                else:
                    num_regressions += 1

        tests[test] = {}
        tests[test]['expected'] = expected
        tests[test]['actual'] = " ".join(actual)
        # FIXME: Set this correctly once https://webkit.org/b/37739 is fixed
        # and only set it if there actually is stderr data.
        tests[test]['has_stderr'] = False

        failure_types = [type(f) for f in result.failures]
        if test_failures.FailureMissingAudio in failure_types:
            tests[test]['is_missing_audio'] = True

        if test_failures.FailureReftestMismatch in failure_types:
            tests[test]['is_reftest'] = True

        for f in result.failures:
            if 'is_reftest' in result.failures:
                tests[test]['is_reftest'] = True

        if test_failures.FailureReftestMismatchDidNotOccur in failure_types:
            tests[test]['is_mismatch_reftest'] = True

        if test_failures.FailureMissingResult in failure_types:
            tests[test]['is_missing_text'] = True

        if test_failures.FailureMissingImage in failure_types or test_failures.FailureMissingImageHash in failure_types:
            tests[test]['is_missing_image'] = True

        if filename in test_timings_map:
            time_seconds = test_timings_map[filename]
            tests[test]['time_ms'] = int(1000 * time_seconds)

    results['tests'] = tests
    results['num_passes'] = num_passes
    results['num_flaky'] = num_flaky
    results['num_regressions'] = num_regressions
    # FIXME: If non-chromium ports start using an expectations file,
    # we should make this check more robust.
    results['uses_expectations_file'] = port_obj.name().find('chromium') != -1
    results['layout_tests_dir'] = port_obj.layout_tests_dir()
    results['has_wdiff'] = port_obj.wdiff_available()
    results['has_pretty_patch'] = port_obj.pretty_patch_available()

    return results


class TestRunInterruptedException(Exception):
    """Raised when a test run should be stopped immediately."""
    def __init__(self, reason):
        self.reason = reason

    def __reduce__(self):
        return self.__class__, (self.reason,)


class TestRunner:
    """A class for managing running a series of tests on a series of layout
    test files."""


    # The per-test timeout in milliseconds, if no --time-out-ms option was
    # given to run_webkit_tests. This should correspond to the default timeout
    # in DumpRenderTree.
    DEFAULT_TEST_TIMEOUT_MS = 6 * 1000

    def __init__(self, port, options, printer):
        """Initialize test runner data structures.

        Args:
          port: an object implementing port-specific
          options: a dictionary of command line options
          printer: a Printer object to record updates to.
        """
        self._port = port
        self._fs = port._filesystem
        self._options = options
        self._printer = printer
        self._message_broker = None

        self.HTTP_SUBDIR = self._fs.join('', 'http', '')
        self.WEBSOCKET_SUBDIR = self._fs.join('', 'websocket', '')
        self.LAYOUT_TESTS_DIRECTORY = "LayoutTests" + self._fs.sep


        # disable wss server. need to install pyOpenSSL on buildbots.
        # self._websocket_secure_server = websocket_server.PyWebSocket(
        #        options.results_directory, use_tls=True, port=9323)

        # a set of test files, and the same tests as a list
        self._test_files = set()
        self._test_files_list = None
        self._result_queue = Queue.Queue()
        self._retrying = False
        self._results_directory = self._port.results_directory()

    def collect_tests(self, args, last_unexpected_results):
        """Find all the files to test.

        Args:
          args: list of test arguments from the command line
          last_unexpected_results: list of unexpected results to retest, if any

        """
        paths = self._strip_test_dir_prefixes(args)
        paths += last_unexpected_results
        if self._options.test_list:
            paths += self._strip_test_dir_prefixes(read_test_files(self._fs, self._options.test_list))
        self._test_files = self._port.tests(paths)

    def _strip_test_dir_prefixes(self, paths):
        return [self._strip_test_dir_prefix(path) for path in paths if path]

    def _strip_test_dir_prefix(self, path):
        if path.startswith(self.LAYOUT_TESTS_DIRECTORY):
            return path[len(self.LAYOUT_TESTS_DIRECTORY):]
        return path

    def lint(self):
        lint_failed = False
        for test_configuration in self._port.all_test_configurations():
            try:
                self.lint_expectations(test_configuration)
            except test_expectations.ParseError:
                lint_failed = True
                self._printer.write("")

        if lint_failed:
            _log.error("Lint failed.")
            return -1

        _log.info("Lint succeeded.")
        return 0

    def lint_expectations(self, config):
        port = self._port
        test_expectations.TestExpectations(
            port,
            None,
            port.test_expectations(),
            config,
            self._options.lint_test_files,
            port.test_expectations_overrides())

    def parse_expectations(self):
        """Parse the expectations from the test_list files and return a data
        structure holding them. Throws an error if the test_list files have
        invalid syntax."""
        port = self._port
        self._expectations = test_expectations.TestExpectations(
            port,
            self._test_files,
            port.test_expectations(),
            port.test_configuration(),
            self._options.lint_test_files,
            port.test_expectations_overrides())

    # FIXME: This method is way too long and needs to be broken into pieces.
    def prepare_lists_and_print_output(self):
        """Create appropriate subsets of test lists and returns a
        ResultSummary object. Also prints expected test counts.
        """

        # Remove skipped - both fixable and ignored - files from the
        # top-level list of files to test.
        num_all_test_files = len(self._test_files)
        self._printer.print_expected("Found:  %d tests" %
                                     (len(self._test_files)))
        if not num_all_test_files:
            _log.critical('No tests to run.')
            return None

        skipped = set()
        if num_all_test_files > 1 and not self._options.force:
            skipped = self._expectations.get_tests_with_result_type(
                           test_expectations.SKIP)
            self._test_files -= skipped

        # Create a sorted list of test files so the subset chunk,
        # if used, contains alphabetically consecutive tests.
        self._test_files_list = list(self._test_files)
        if self._options.randomize_order:
            random.shuffle(self._test_files_list)
        else:
            self._test_files_list.sort()

        # If the user specifies they just want to run a subset of the tests,
        # just grab a subset of the non-skipped tests.
        if self._options.run_chunk or self._options.run_part:
            chunk_value = self._options.run_chunk or self._options.run_part
            test_files = self._test_files_list
            try:
                (chunk_num, chunk_len) = chunk_value.split(":")
                chunk_num = int(chunk_num)
                assert(chunk_num >= 0)
                test_size = int(chunk_len)
                assert(test_size > 0)
            except:
                _log.critical("invalid chunk '%s'" % chunk_value)
                return None

            # Get the number of tests
            num_tests = len(test_files)

            # Get the start offset of the slice.
            if self._options.run_chunk:
                chunk_len = test_size
                # In this case chunk_num can be really large. We need
                # to make the slave fit in the current number of tests.
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
                    rounded_tests = (num_tests + test_size -
                                     (num_tests % test_size))

                chunk_len = rounded_tests / test_size
                slice_start = chunk_len * (chunk_num - 1)
                # It does not mind if we go over test_size.

            # Get the end offset of the slice.
            slice_end = min(num_tests, slice_start + chunk_len)

            files = test_files[slice_start:slice_end]

            tests_run_msg = 'Running: %d tests (chunk slice [%d:%d] of %d)' % (
                (slice_end - slice_start), slice_start, slice_end, num_tests)
            self._printer.print_expected(tests_run_msg)

            # If we reached the end and we don't have enough tests, we run some
            # from the beginning.
            if slice_end - slice_start < chunk_len:
                extra = chunk_len - (slice_end - slice_start)
                extra_msg = ('   last chunk is partial, appending [0:%d]' %
                            extra)
                self._printer.print_expected(extra_msg)
                tests_run_msg += "\n" + extra_msg
                files.extend(test_files[0:extra])
            tests_run_filename = self._fs.join(self._results_directory, "tests_run.txt")
            self._fs.write_text_file(tests_run_filename, tests_run_msg)

            len_skip_chunk = int(len(files) * len(skipped) /
                                 float(len(self._test_files)))
            skip_chunk_list = list(skipped)[0:len_skip_chunk]
            skip_chunk = set(skip_chunk_list)

            # Update expectations so that the stats are calculated correctly.
            # We need to pass a list that includes the right # of skipped files
            # to ParseExpectations so that ResultSummary() will get the correct
            # stats. So, we add in the subset of skipped files, and then
            # subtract them back out.
            self._test_files_list = files + skip_chunk_list
            self._test_files = set(self._test_files_list)

            self.parse_expectations()

            self._test_files = set(files)
            self._test_files_list = files
        else:
            skip_chunk = skipped

        result_summary = ResultSummary(self._expectations,
            self._test_files | skip_chunk)
        self._print_expected_results_of_type(result_summary,
            test_expectations.PASS, "passes")
        self._print_expected_results_of_type(result_summary,
            test_expectations.FAIL, "failures")
        self._print_expected_results_of_type(result_summary,
            test_expectations.FLAKY, "flaky")
        self._print_expected_results_of_type(result_summary,
            test_expectations.SKIP, "skipped")

        if self._options.force:
            self._printer.print_expected('Running all tests, including '
                                         'skips (--force)')
        else:
            # Note that we don't actually run the skipped tests (they were
            # subtracted out of self._test_files, above), but we stub out the
            # results here so the statistics can remain accurate.
            for test in skip_chunk:
                result = test_results.TestResult(test)
                result.type = test_expectations.SKIP
                result_summary.add(result, expected=True)
        self._printer.print_expected('')

        # Check to make sure we didn't filter out all of the tests.
        if not len(self._test_files):
            _log.info("All tests are being skipped")
            return None

        return result_summary

    def _get_dir_for_test_file(self, test_file):
        """Returns the highest-level directory by which to shard the given
        test file."""
        index = test_file.rfind(self._fs.sep + self.LAYOUT_TESTS_DIRECTORY)

        test_file = test_file[index + len(self.LAYOUT_TESTS_DIRECTORY):]
        test_file_parts = test_file.split(self._fs.sep, 1)
        directory = test_file_parts[0]
        test_file = test_file_parts[1]

        # The http tests are very stable on mac/linux.
        # TODO(ojan): Make the http server on Windows be apache so we can
        # turn shard the http tests there as well. Switching to apache is
        # what made them stable on linux/mac.
        return_value = directory
        while ((directory != 'http' or sys.platform in ('darwin', 'linux2'))
                and test_file.find(self._fs.sep) >= 0):
            test_file_parts = test_file.split(self._fs.sep, 1)
            directory = test_file_parts[0]
            return_value = self._fs.join(return_value, directory)
            test_file = test_file_parts[1]

        return return_value

    def _get_test_input_for_file(self, test_file):
        """Returns the appropriate TestInput object for the file. Mostly this
        is used for looking up the timeout value (in ms) to use for the given
        test."""
        if self._test_is_slow(test_file):
            return TestInput(test_file, self._options.slow_time_out_ms)
        return TestInput(test_file, self._options.time_out_ms)

    def _test_requires_lock(self, test_file):
        """Return True if the test needs to be locked when
        running multiple copies of NRWTs."""
        split_path = test_file.split(self._port._filesystem.sep)
        return 'http' in split_path or 'websocket' in split_path

    def _test_is_slow(self, test_file):
        return self._expectations.has_modifier(test_file,
                                               test_expectations.SLOW)

    def _shard_tests(self, test_files, use_real_shards):
        """Groups tests into batches.
        This helps ensure that tests that depend on each other (aka bad tests!)
        continue to run together as most cross-tests dependencies tend to
        occur within the same directory. If use_real_shards is False, we
        put each (non-HTTP/websocket) test into its own shard for maximum
        concurrency instead of trying to do any sort of real sharding.

        Return:
            A list of lists of TestInput objects.
        """
        # FIXME: when we added http locking, we changed how this works such
        # that we always lump all of the HTTP threads into a single shard.
        # That will slow down experimental-fully-parallel, but it's unclear
        # what the best alternative is completely revamping how we track
        # when to grab the lock.

        test_lists = []
        tests_to_http_lock = []
        if not use_real_shards:
            for test_file in test_files:
                test_input = self._get_test_input_for_file(test_file)
                if self._test_requires_lock(test_file):
                    tests_to_http_lock.append(test_input)
                else:
                    test_lists.append((".", [test_input]))
        else:
            tests_by_dir = {}
            for test_file in test_files:
                directory = self._get_dir_for_test_file(test_file)
                test_input = self._get_test_input_for_file(test_file)
                if self._test_requires_lock(test_file):
                    tests_to_http_lock.append(test_input)
                else:
                    tests_by_dir.setdefault(directory, [])
                    tests_by_dir[directory].append(test_input)
            # Sort by the number of tests in the dir so that the ones with the
            # most tests get run first in order to maximize parallelization.
            # Number of tests is a good enough, but not perfect, approximation
            # of how long that set of tests will take to run. We can't just use
            # a PriorityQueue until we move to Python 2.6.
            for directory in tests_by_dir:
                test_list = tests_by_dir[directory]
                test_list_tuple = (directory, test_list)
                test_lists.append(test_list_tuple)
            test_lists.sort(lambda a, b: cmp(len(b[1]), len(a[1])))

        # Put the http tests first. There are only a couple hundred of them,
        # but each http test takes a very long time to run, so sorting by the
        # number of tests doesn't accurately capture how long they take to run.
        if tests_to_http_lock:
            test_lists.insert(0, ("tests_to_http_lock", tests_to_http_lock))

        return test_lists

    def _contains_tests(self, subdir):
        for test_file in self._test_files:
            if test_file.find(subdir) >= 0:
                return True
        return False

    def _num_workers(self, num_shards):
        num_workers = min(int(self._options.child_processes), num_shards)
        driver_name = self._port.driver_name()
        if num_workers == 1:
            self._printer.print_config("Running 1 %s over %s" %
                (driver_name, grammar.pluralize('shard', num_shards)))
        else:
            self._printer.print_config("Running %d %ss in parallel over %d shards" %
                (num_workers, driver_name, num_shards))
        return num_workers

    def _run_tests(self, file_list, result_summary):
        """Runs the tests in the file_list.

        Return: A tuple (interrupted, keyboard_interrupted, thread_timings,
            test_timings, individual_test_timings)
            interrupted is whether the run was interrupted
            keyboard_interrupted is whether the interruption was because someone
              typed Ctrl^C
            thread_timings is a list of dicts with the total runtime
              of each thread with 'name', 'num_tests', 'total_time' properties
            test_timings is a list of timings for each sharded subdirectory
              of the form [time, directory_name, num_tests]
            individual_test_timings is a list of run times for each test
              in the form {filename:filename, test_run_time:test_run_time}
            result_summary: summary object to populate with the results
        """
        raise NotImplementedError()

    def update(self):
        self.update_summary(self._current_result_summary)

    def _collect_timing_info(self, threads):
        test_timings = {}
        individual_test_timings = []
        thread_timings = []

        for thread in threads:
            thread_timings.append({'name': thread.getName(),
                                   'num_tests': thread.get_num_tests(),
                                   'total_time': thread.get_total_time()})
            test_timings.update(thread.get_test_group_timing_stats())
            individual_test_timings.extend(thread.get_test_results())

        return (thread_timings, test_timings, individual_test_timings)

    def needs_http(self):
        """Returns whether the test runner needs an HTTP server."""
        return self._contains_tests(self.HTTP_SUBDIR)

    def needs_websocket(self):
        """Returns whether the test runner needs a WEBSOCKET server."""
        return self._contains_tests(self.WEBSOCKET_SUBDIR)

    def set_up_run(self):
        """Configures the system to be ready to run tests.

        Returns a ResultSummary object if we should continue to run tests,
        or None if we should abort.

        """
        # This must be started before we check the system dependencies,
        # since the helper may do things to make the setup correct.
        self._printer.print_update("Starting helper ...")
        self._port.start_helper()

        # Check that the system dependencies (themes, fonts, ...) are correct.
        if not self._options.nocheck_sys_deps:
            self._printer.print_update("Checking system dependencies ...")
            if not self._port.check_sys_deps(self.needs_http()):
                self._port.stop_helper()
                return None

        if self._options.clobber_old_results:
            self._clobber_old_results()

        # Create the output directory if it doesn't already exist.
        self._port.maybe_make_directory(self._results_directory)

        self._port.setup_test_run()

        self._printer.print_update("Preparing tests ...")
        result_summary = self.prepare_lists_and_print_output()
        if not result_summary:
            return None

        return result_summary

    def run(self, result_summary):
        """Run all our tests on all our test files.

        For each test file, we run each test type. If there are any failures,
        we collect them for reporting.

        Args:
          result_summary: a summary object tracking the test results.

        Return:
          The number of unexpected results (0 == success)
        """
        # gather_test_files() must have been called first to initialize us.
        # If we didn't find any files to test, we've errored out already in
        # prepare_lists_and_print_output().
        assert(len(self._test_files))

        start_time = time.time()

        interrupted, keyboard_interrupted, thread_timings, test_timings, \
            individual_test_timings = (
            self._run_tests(self._test_files_list, result_summary))

        # We exclude the crashes from the list of results to retry, because
        # we want to treat even a potentially flaky crash as an error.
        failures = self._get_failures(result_summary, include_crashes=False)
        retry_summary = result_summary
        while (len(failures) and self._options.retry_failures and
            not self._retrying and not interrupted):
            _log.info('')
            _log.info("Retrying %d unexpected failure(s) ..." % len(failures))
            _log.info('')
            self._retrying = True
            retry_summary = ResultSummary(self._expectations, failures.keys())
            # Note that we intentionally ignore the return value here.
            self._run_tests(failures.keys(), retry_summary)
            failures = self._get_failures(retry_summary, include_crashes=True)

        end_time = time.time()

        self._print_timing_statistics(end_time - start_time,
                                      thread_timings, test_timings,
                                      individual_test_timings,
                                      result_summary)

        self._print_result_summary(result_summary)

        sys.stdout.flush()
        sys.stderr.flush()

        self._printer.print_one_line_summary(result_summary.total,
                                             result_summary.expected,
                                             result_summary.unexpected)

        unexpected_results = summarize_results(self._port,
            self._expectations, result_summary, retry_summary, individual_test_timings, only_unexpected=True)
        self._printer.print_unexpected_results(unexpected_results)

        # FIXME: remove record_results. It's just used for testing. There's no need
        # for it to be a commandline argument.
        if (self._options.record_results and not self._options.dry_run and
            not keyboard_interrupted):
            # Write the same data to log files and upload generated JSON files
            # to appengine server.
            summarized_results = summarize_results(self._port,
                self._expectations, result_summary, retry_summary, individual_test_timings, only_unexpected=False)
            self._upload_json_files(unexpected_results, summarized_results, result_summary,
                                    individual_test_timings)

        # Write the summary to disk (results.html) and display it if requested.
        if not self._options.dry_run:
            self._copy_results_html_file()
            if self._options.show_results:
                self._show_results_html_file(result_summary)

        # Now that we've completed all the processing we can, we re-raise
        # a KeyboardInterrupt if necessary so the caller can handle it.
        if keyboard_interrupted:
            raise KeyboardInterrupt

        # Ignore flaky failures and unexpected passes so we don't turn the
        # bot red for those.
        return unexpected_results['num_regressions']

    def clean_up_run(self):
        """Restores the system after we're done running tests."""

        _log.debug("flushing stdout")
        sys.stdout.flush()
        _log.debug("flushing stderr")
        sys.stderr.flush()
        _log.debug("stopping helper")
        self._port.stop_helper()

    def update_summary(self, result_summary):
        """Update the summary and print results with any completed tests."""
        while True:
            try:
                result = test_results.TestResult.loads(self._result_queue.get_nowait())
            except Queue.Empty:
                return

            self._update_summary_with_result(result_summary, result)

    def _update_summary_with_result(self, result_summary, result):
        expected = self._expectations.matches_an_expected_result(
            result.filename, result.type, self._options.pixel_tests)
        result_summary.add(result, expected)
        exp_str = self._expectations.get_expectations_string(
            result.filename)
        got_str = self._expectations.expectation_to_string(result.type)
        self._printer.print_test_result(result, expected, exp_str, got_str)
        self._printer.print_progress(result_summary, self._retrying,
                                        self._test_files_list)

        def interrupt_if_at_failure_limit(limit, count, message):
            if limit and count >= limit:
                raise TestRunInterruptedException(message % count)

        interrupt_if_at_failure_limit(
            self._options.exit_after_n_failures,
            result_summary.unexpected_failures,
            "Aborting run since %d failures were reached")
        interrupt_if_at_failure_limit(
            self._options.exit_after_n_crashes_or_timeouts,
            result_summary.unexpected_crashes_or_timeouts,
            "Aborting run since %d crashes or timeouts were reached")

    def _clobber_old_results(self):
        # Just clobber the actual test results directories since the other
        # files in the results directory are explicitly used for cross-run
        # tracking.
        self._printer.print_update("Clobbering old results in %s" %
                                   self._results_directory)
        layout_tests_dir = self._port.layout_tests_dir()
        possible_dirs = self._port.test_dirs()
        for dirname in possible_dirs:
            if self._fs.isdir(self._fs.join(layout_tests_dir, dirname)):
                self._fs.rmtree(self._fs.join(self._results_directory, dirname))

    def _get_failures(self, result_summary, include_crashes):
        """Filters a dict of results and returns only the failures.

        Args:
          result_summary: the results of the test run
          include_crashes: whether crashes are included in the output.
            We use False when finding the list of failures to retry
            to see if the results were flaky. Although the crashes may also be
            flaky, we treat them as if they aren't so that they're not ignored.
        Returns:
          a dict of files -> results
        """
        failed_results = {}
        for test, result in result_summary.unexpected_results.iteritems():
            if (result.type == test_expectations.PASS or
                result.type == test_expectations.CRASH and not include_crashes):
                continue
            failed_results[test] = result.type

        return failed_results

    def _char_for_result(self, result):
        result = result.lower()
        if result in TestExpectationsFile.EXPECTATIONS:
            result_enum_value = TestExpectationsFile.EXPECTATIONS[result]
        else:
            result_enum_value = TestExpectationsFile.MODIFIERS[result]
        return json_layout_results_generator.JSONLayoutResultsGenerator.FAILURE_TO_CHAR[result_enum_value]

    def _upload_json_files(self, unexpected_results, summarized_results, result_summary,
                           individual_test_timings):
        """Writes the results of the test run as JSON files into the results
        dir and upload the files to the appengine server.

        There are three different files written into the results dir:
          unexpected_results.json: A short list of any unexpected results.
            This is used by the buildbots to display results.
          expectations.json: This is used by the flakiness dashboard.
          results.json: A full list of the results - used by the flakiness
            dashboard and the aggregate results dashboard.

        Args:
          unexpected_results: dict of unexpected results
          summarized_results: dict of results
          result_summary: full summary object
          individual_test_timings: list of test times (used by the flakiness
            dashboard).
        """
        _log.debug("Writing JSON files in %s." % self._results_directory)

        unexpected_json_path = self._fs.join(self._results_directory, "unexpected_results.json")
        json_results_generator.write_json(self._fs, unexpected_results, unexpected_json_path)

        full_results_path = self._fs.join(self._results_directory, "full_results.json")
        json_results_generator.write_json(self._fs, summarized_results, full_results_path)

        # Write a json file of the test_expectations.txt file for the layout
        # tests dashboard.
        expectations_path = self._fs.join(self._results_directory, "expectations.json")
        expectations_json = \
            self._expectations.get_expectations_json_for_all_platforms()
        self._fs.write_text_file(expectations_path,
                                 u"ADD_EXPECTATIONS(%s);" % expectations_json)

        generator = json_layout_results_generator.JSONLayoutResultsGenerator(
            self._port, self._options.builder_name, self._options.build_name,
            self._options.build_number, self._results_directory,
            BUILDER_BASE_URL, individual_test_timings,
            self._expectations, result_summary, self._test_files_list,
            self._options.test_results_server,
            "layout-tests",
            self._options.master_name)

        _log.debug("Finished writing JSON files.")

        json_files = ["expectations.json", "incremental_results.json", "full_results.json"]

        generator.upload_json_files(json_files)

    def _print_config(self):
        """Prints the configuration for the test run."""
        p = self._printer
        p.print_config("Using port '%s'" % self._port.name())
        p.print_config("Test configuration: %s" % self._port.test_configuration())
        p.print_config("Placing test results in %s" % self._results_directory)
        if self._options.new_baseline:
            p.print_config("Placing new baselines in %s" %
                           self._port.baseline_path())
        p.print_config("Using %s build" % self._options.configuration)
        if self._options.pixel_tests:
            p.print_config("Pixel tests enabled")
        else:
            p.print_config("Pixel tests disabled")

        p.print_config("Regular timeout: %s, slow test timeout: %s" %
                       (self._options.time_out_ms,
                        self._options.slow_time_out_ms))

        p.print_config('Command line: ' +
                       ' '.join(self._port.driver_cmd_line()))
        p.print_config("Worker model: %s" % self._options.worker_model)
        p.print_config("")

    def _print_expected_results_of_type(self, result_summary,
                                        result_type, result_type_str):
        """Print the number of the tests in a given result class.

        Args:
          result_summary - the object containing all the results to report on
          result_type - the particular result type to report in the summary.
          result_type_str - a string description of the result_type.
        """
        tests = self._expectations.get_tests_with_result_type(result_type)
        now = result_summary.tests_by_timeline[test_expectations.NOW]
        wontfix = result_summary.tests_by_timeline[test_expectations.WONTFIX]

        # We use a fancy format string in order to print the data out in a
        # nicely-aligned table.
        fmtstr = ("Expect: %%5d %%-8s (%%%dd now, %%%dd wontfix)"
                  % (self._num_digits(now), self._num_digits(wontfix)))
        self._printer.print_expected(fmtstr %
            (len(tests), result_type_str, len(tests & now), len(tests & wontfix)))

    def _num_digits(self, num):
        """Returns the number of digits needed to represent the length of a
        sequence."""
        ndigits = 1
        if len(num):
            ndigits = int(math.log10(len(num))) + 1
        return ndigits

    def _print_timing_statistics(self, total_time, thread_timings,
                               directory_test_timings, individual_test_timings,
                               result_summary):
        """Record timing-specific information for the test run.

        Args:
          total_time: total elapsed time (in seconds) for the test run
          thread_timings: wall clock time each thread ran for
          directory_test_timings: timing by directory
          individual_test_timings: timing by file
          result_summary: summary object for the test run
        """
        self._printer.print_timing("Test timing:")
        self._printer.print_timing("  %6.2f total testing time" % total_time)
        self._printer.print_timing("")
        self._printer.print_timing("Thread timing:")
        cuml_time = 0
        for t in thread_timings:
            self._printer.print_timing("    %10s: %5d tests, %6.2f secs" %
                  (t['name'], t['num_tests'], t['total_time']))
            cuml_time += t['total_time']
        self._printer.print_timing("   %6.2f cumulative, %6.2f optimal" %
              (cuml_time, cuml_time / int(self._options.child_processes)))
        self._printer.print_timing("")

        self._print_aggregate_test_statistics(individual_test_timings)
        self._print_individual_test_times(individual_test_timings,
                                          result_summary)
        self._print_directory_timings(directory_test_timings)

    def _print_aggregate_test_statistics(self, individual_test_timings):
        """Prints aggregate statistics (e.g. median, mean, etc.) for all tests.
        Args:
          individual_test_timings: List of TestResults for all tests.
        """
        times_for_dump_render_tree = [test_stats.test_run_time for test_stats in individual_test_timings]
        self._print_statistics_for_test_timings("PER TEST TIME IN TESTSHELL (seconds):",
                                                times_for_dump_render_tree)

    def _print_individual_test_times(self, individual_test_timings,
                                  result_summary):
        """Prints the run times for slow, timeout and crash tests.
        Args:
          individual_test_timings: List of TestStats for all tests.
          result_summary: summary object for test run
        """
        # Reverse-sort by the time spent in DumpRenderTree.
        individual_test_timings.sort(lambda a, b:
            cmp(b.test_run_time, a.test_run_time))

        num_printed = 0
        slow_tests = []
        timeout_or_crash_tests = []
        unexpected_slow_tests = []
        for test_tuple in individual_test_timings:
            filename = test_tuple.filename
            is_timeout_crash_or_slow = False
            if self._test_is_slow(filename):
                is_timeout_crash_or_slow = True
                slow_tests.append(test_tuple)

            if filename in result_summary.failures:
                result = result_summary.results[filename].type
                if (result == test_expectations.TIMEOUT or
                    result == test_expectations.CRASH):
                    is_timeout_crash_or_slow = True
                    timeout_or_crash_tests.append(test_tuple)

            if (not is_timeout_crash_or_slow and
                num_printed < printing.NUM_SLOW_TESTS_TO_LOG):
                num_printed = num_printed + 1
                unexpected_slow_tests.append(test_tuple)

        self._printer.print_timing("")
        self._print_test_list_timing("%s slowest tests that are not "
            "marked as SLOW and did not timeout/crash:" %
            printing.NUM_SLOW_TESTS_TO_LOG, unexpected_slow_tests)
        self._printer.print_timing("")
        self._print_test_list_timing("Tests marked as SLOW:", slow_tests)
        self._printer.print_timing("")
        self._print_test_list_timing("Tests that timed out or crashed:",
                                     timeout_or_crash_tests)
        self._printer.print_timing("")

    def _print_test_list_timing(self, title, test_list):
        """Print timing info for each test.

        Args:
          title: section heading
          test_list: tests that fall in this section
        """
        if self._printer.disabled('slowest'):
            return

        self._printer.print_timing(title)
        for test_tuple in test_list:
            filename = test_tuple.filename[len(
                self._port.layout_tests_dir()) + 1:]
            filename = filename.replace('\\', '/')
            test_run_time = round(test_tuple.test_run_time, 1)
            self._printer.print_timing("  %s took %s seconds" %
                                       (filename, test_run_time))

    def _print_directory_timings(self, directory_test_timings):
        """Print timing info by directory for any directories that
        take > 10 seconds to run.

        Args:
          directory_test_timing: time info for each directory
        """
        timings = []
        for directory in directory_test_timings:
            num_tests, time_for_directory = directory_test_timings[directory]
            timings.append((round(time_for_directory, 1), directory,
                            num_tests))
        timings.sort()

        self._printer.print_timing("Time to process slowest subdirectories:")
        min_seconds_to_print = 10
        for timing in timings:
            if timing[0] > min_seconds_to_print:
                self._printer.print_timing(
                    "  %s took %s seconds to run %s tests." % (timing[1],
                    timing[0], timing[2]))
        self._printer.print_timing("")

    def _print_statistics_for_test_timings(self, title, timings):
        """Prints the median, mean and standard deviation of the values in
        timings.

        Args:
          title: Title for these timings.
          timings: A list of floats representing times.
        """
        self._printer.print_timing(title)
        timings.sort()

        num_tests = len(timings)
        if not num_tests:
            return
        percentile90 = timings[int(.9 * num_tests)]
        percentile99 = timings[int(.99 * num_tests)]

        if num_tests % 2 == 1:
            median = timings[((num_tests - 1) / 2) - 1]
        else:
            lower = timings[num_tests / 2 - 1]
            upper = timings[num_tests / 2]
            median = (float(lower + upper)) / 2

        mean = sum(timings) / num_tests

        for time in timings:
            sum_of_deviations = math.pow(time - mean, 2)

        std_deviation = math.sqrt(sum_of_deviations / num_tests)
        self._printer.print_timing("  Median:          %6.3f" % median)
        self._printer.print_timing("  Mean:            %6.3f" % mean)
        self._printer.print_timing("  90th percentile: %6.3f" % percentile90)
        self._printer.print_timing("  99th percentile: %6.3f" % percentile99)
        self._printer.print_timing("  Standard dev:    %6.3f" % std_deviation)
        self._printer.print_timing("")

    def _print_result_summary(self, result_summary):
        """Print a short summary about how many tests passed.

        Args:
          result_summary: information to log
        """
        failed = len(result_summary.failures)
        skipped = len(
            result_summary.tests_by_expectation[test_expectations.SKIP])
        total = result_summary.total
        passed = total - failed - skipped
        pct_passed = 0.0
        if total > 0:
            pct_passed = float(passed) * 100 / total

        self._printer.print_actual("")
        self._printer.print_actual("=> Results: %d/%d tests passed (%.1f%%)" %
                     (passed, total, pct_passed))
        self._printer.print_actual("")
        self._print_result_summary_entry(result_summary,
            test_expectations.NOW, "Tests to be fixed")

        self._printer.print_actual("")
        self._print_result_summary_entry(result_summary,
            test_expectations.WONTFIX,
            "Tests that will only be fixed if they crash (WONTFIX)")
        self._printer.print_actual("")

    def _print_result_summary_entry(self, result_summary, timeline,
                                    heading):
        """Print a summary block of results for a particular timeline of test.

        Args:
          result_summary: summary to print results for
          timeline: the timeline to print results for (NOT, WONTFIX, etc.)
          heading: a textual description of the timeline
        """
        total = len(result_summary.tests_by_timeline[timeline])
        not_passing = (total -
           len(result_summary.tests_by_expectation[test_expectations.PASS] &
               result_summary.tests_by_timeline[timeline]))
        self._printer.print_actual("=> %s (%d):" % (heading, not_passing))

        for result in TestExpectationsFile.EXPECTATION_ORDER:
            if result == test_expectations.PASS:
                continue
            results = (result_summary.tests_by_expectation[result] &
                       result_summary.tests_by_timeline[timeline])
            desc = TestExpectationsFile.EXPECTATION_DESCRIPTIONS[result]
            if not_passing and len(results):
                pct = len(results) * 100.0 / not_passing
                self._printer.print_actual("  %5d %-24s (%4.1f%%)" %
                    (len(results), desc[len(results) != 1], pct))

    def _copy_results_html_file(self):
        base_dir = self._port.path_from_webkit_base('Tools', 'Scripts', 'webkitpy', 'layout_tests', 'layout_package')
        results_file = self._fs.join(base_dir, 'json_results.html')
        # FIXME: What should we do if this doesn't exist (e.g., in unit tests)?
        if self._fs.exists(results_file):
            self._fs.copyfile(results_file, self._fs.join(self._results_directory, "results.html"))

    def _show_results_html_file(self, result_summary):
        """Shows the results.html page."""
        if self._options.full_results_html:
            test_files = result_summary.failures.keys()
        else:
            unexpected_failures = self._get_failures(result_summary, include_crashes=True)
            test_files = unexpected_failures.keys()

        if not len(test_files):
            return

        results_filename = self._fs.join(self._results_directory, "results.html")
        self._port.show_results_html_file(results_filename)


def read_test_files(fs, files):
    tests = []
    for file in files:
        try:
            file_contents = fs.read_text_file(file).split('\n')
            for line in file_contents:
                line = test_expectations.strip_comments(line)
                if line:
                    tests.append(line)
        except IOError, e:
            if e.errno == errno.ENOENT:
                _log.critical('')
                _log.critical('--test-list file "%s" not found' % file)
            raise
    return tests
