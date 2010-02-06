#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
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

"""Run layout tests using the test_shell.

This is a port of the existing webkit test script run-webkit-tests.

The TestRunner class runs a series of tests (TestType interface) against a set
of test files.  If a test file fails a TestType, it returns a list TestFailure
objects to the TestRunner.  The TestRunner then aggregates the TestFailures to
create a final report.

This script reads several files, if they exist in the test_lists subdirectory
next to this script itself.  Each should contain a list of paths to individual
tests or entire subdirectories of tests, relative to the outermost test
directory.  Entire lines starting with '//' (comments) will be ignored.

For details of the files' contents and purposes, see test_lists/README.
"""

import errno
import glob
import logging
import math
import optparse
import os
import Queue
import random
import re
import shutil
import sys
import time
import traceback

import simplejson

from layout_package import test_expectations
from layout_package import json_layout_results_generator
from layout_package import metered_stream
from layout_package import test_failures
from layout_package import test_shell_thread
from layout_package import test_files
from test_types import fuzzy_image_diff
from test_types import image_diff
from test_types import test_type_base
from test_types import text_diff

import port

# Indicates that we want detailed progress updates in the output (prints
# directory-by-directory feedback).
LOG_DETAILED_PROGRESS = 'detailed-progress'

# Log any unexpected results while running (instead of just at the end).
LOG_UNEXPECTED = 'unexpected'

# Builder base URL where we have the archived test results.
BUILDER_BASE_URL = "http://build.chromium.org/buildbot/layout_test_results/"

TestExpectationsFile = test_expectations.TestExpectationsFile


class TestInfo:
    """Groups information about a test for easy passing of data."""

    def __init__(self, port, filename, timeout):
        """Generates the URI and stores the filename and timeout for this test.
        Args:
          filename: Full path to the test.
          timeout: Timeout for running the test in TestShell.
          """
        self.filename = filename
        self.uri = port.filename_to_uri(filename)
        self.timeout = timeout
        expected_hash_file = port.expected_filename(filename, '.checksum')
        try:
            self.image_hash = open(expected_hash_file, "r").read()
        except IOError, e:
            if errno.ENOENT != e.errno:
                raise
            self.image_hash = None


class ResultSummary(object):
    """A class for partitioning the test results we get into buckets.

    This class is basically a glorified struct and it's private to this file
    so we don't bother with any information hiding."""

    def __init__(self, expectations, test_files):
        self.total = len(test_files)
        self.remaining = self.total
        self.expectations = expectations
        self.expected = 0
        self.unexpected = 0
        self.tests_by_expectation = {}
        self.tests_by_timeline = {}
        self.results = {}
        self.unexpected_results = {}
        self.failures = {}
        self.tests_by_expectation[test_expectations.SKIP] = set()
        for expectation in TestExpectationsFile.EXPECTATIONS.values():
            self.tests_by_expectation[expectation] = set()
        for timeline in TestExpectationsFile.TIMELINES.values():
            self.tests_by_timeline[timeline] = (
                expectations.get_tests_with_timeline(timeline))

    def add(self, test, failures, result, expected):
        """Add a result into the appropriate bin.

        Args:
          test: test file name
          failures: list of failure objects from test execution
          result: result of test (PASS, IMAGE, etc.).
          expected: whether the result was what we expected it to be.
        """

        self.tests_by_expectation[result].add(test)
        self.results[test] = result
        self.remaining -= 1
        if len(failures):
            self.failures[test] = failures
        if expected:
            self.expected += 1
        else:
            self.unexpected_results[test] = result
            self.unexpected += 1


class TestRunner:
    """A class for managing running a series of tests on a series of layout
    test files."""

    HTTP_SUBDIR = os.sep.join(['', 'http', ''])
    WEBSOCKET_SUBDIR = os.sep.join(['', 'websocket', ''])

    # The per-test timeout in milliseconds, if no --time-out-ms option was
    # given to run_webkit_tests. This should correspond to the default timeout
    # in test_shell.exe.
    DEFAULT_TEST_TIMEOUT_MS = 6 * 1000

    NUM_RETRY_ON_UNEXPECTED_FAILURE = 1

    def __init__(self, port, options, meter):
        """Initialize test runner data structures.

        Args:
          port: an object implementing port-specific
          options: a dictionary of command line options
          meter: a MeteredStream object to record updates to.
        """
        self._port = port
        self._options = options
        self._meter = meter

        # disable wss server. need to install pyOpenSSL on buildbots.
        # self._websocket_secure_server = websocket_server.PyWebSocket(
        #        options.results_directory, use_tls=True, port=9323)

        # a list of TestType objects
        self._test_types = []

        # a set of test files, and the same tests as a list
        self._test_files = set()
        self._test_files_list = None
        self._result_queue = Queue.Queue()

        # These are used for --log detailed-progress to track status by
        # directory.
        self._current_dir = None
        self._current_progress_str = ""
        self._current_test_number = 0

    def __del__(self):
        logging.debug("flushing stdout")
        sys.stdout.flush()
        logging.debug("flushing stderr")
        sys.stderr.flush()
        logging.debug("stopping http server")
        self._port.stop_http_server()
        logging.debug("stopping websocket server")
        self._port.stop_websocket_server()

    def gather_file_paths(self, paths):
        """Find all the files to test.

        Args:
          paths: a list of globs to use instead of the defaults."""
        self._test_files = test_files.gather_test_files(self._port, paths)

    def parse_expectations(self, test_platform_name, is_debug_mode):
        """Parse the expectations from the test_list files and return a data
        structure holding them. Throws an error if the test_list files have
        invalid syntax."""
        if self._options.lint_test_files:
            test_files = None
        else:
            test_files = self._test_files

        try:
            expectations_str = self._port.test_expectations()
            self._expectations = test_expectations.TestExpectations(
                self._port, test_files, expectations_str, test_platform_name,
                is_debug_mode, self._options.lint_test_files)
            return self._expectations
        except Exception, err:
            if self._options.lint_test_files:
                print str(err)
            else:
                raise err

    def prepare_lists_and_print_output(self, write):
        """Create appropriate subsets of test lists and returns a
        ResultSummary object. Also prints expected test counts.

        Args:
          write: A callback to write info to (e.g., a LoggingWriter) or
             sys.stdout.write.
        """

        # Remove skipped - both fixable and ignored - files from the
        # top-level list of files to test.
        num_all_test_files = len(self._test_files)
        write("Found:  %d tests" % (len(self._test_files)))
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
                logging.critical("invalid chunk '%s'" % chunk_value)
                sys.exit(1)

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
            write(tests_run_msg)

            # If we reached the end and we don't have enough tests, we run some
            # from the beginning.
            if (self._options.run_chunk and
                (slice_end - slice_start < chunk_len)):
                extra = 1 + chunk_len - (slice_end - slice_start)
                extra_msg = ('   last chunk is partial, appending [0:%d]' %
                            extra)
                write(extra_msg)
                tests_run_msg += "\n" + extra_msg
                files.extend(test_files[0:extra])
            tests_run_filename = os.path.join(self._options.results_directory,
                                              "tests_run.txt")
            tests_run_file = open(tests_run_filename, "w")
            tests_run_file.write(tests_run_msg + "\n")
            tests_run_file.close()

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

            self._expectations = self.parse_expectations(
                self._port.test_platform_name(),
                self._options.target == 'Debug')

            self._test_files = set(files)
            self._test_files_list = files
        else:
            skip_chunk = skipped

        result_summary = ResultSummary(self._expectations,
            self._test_files | skip_chunk)
        self._print_expected_results_of_type(write, result_summary,
            test_expectations.PASS, "passes")
        self._print_expected_results_of_type(write, result_summary,
            test_expectations.FAIL, "failures")
        self._print_expected_results_of_type(write, result_summary,
            test_expectations.FLAKY, "flaky")
        self._print_expected_results_of_type(write, result_summary,
            test_expectations.SKIP, "skipped")


        if self._options.force:
            write('Running all tests, including skips (--force)')
        else:
            # Note that we don't actually run the skipped tests (they were
            # subtracted out of self._test_files, above), but we stub out the
            # results here so the statistics can remain accurate.
            for test in skip_chunk:
                result_summary.add(test, [], test_expectations.SKIP,
                                   expected=True)
        write("")

        return result_summary

    def add_test_type(self, test_type):
        """Add a TestType to the TestRunner."""
        self._test_types.append(test_type)

    def _get_dir_for_test_file(self, test_file):
        """Returns the highest-level directory by which to shard the given
        test file."""
        index = test_file.rfind(os.sep + 'LayoutTests' + os.sep)

        test_file = test_file[index + len('LayoutTests/'):]
        test_file_parts = test_file.split(os.sep, 1)
        directory = test_file_parts[0]
        test_file = test_file_parts[1]

        # The http tests are very stable on mac/linux.
        # TODO(ojan): Make the http server on Windows be apache so we can
        # turn shard the http tests there as well. Switching to apache is
        # what made them stable on linux/mac.
        return_value = directory
        while ((directory != 'http' or sys.platform in ('darwin', 'linux2'))
                and test_file.find(os.sep) >= 0):
            test_file_parts = test_file.split(os.sep, 1)
            directory = test_file_parts[0]
            return_value = os.path.join(return_value, directory)
            test_file = test_file_parts[1]

        return return_value

    def _get_test_info_for_file(self, test_file):
        """Returns the appropriate TestInfo object for the file. Mostly this
        is used for looking up the timeout value (in ms) to use for the given
        test."""
        if self._expectations.has_modifier(test_file, test_expectations.SLOW):
            return TestInfo(self._port, test_file,
                            self._options.slow_time_out_ms)
        return TestInfo(self._port, test_file, self._options.time_out_ms)

    def _get_test_file_queue(self, test_files):
        """Create the thread safe queue of lists of (test filenames, test URIs)
        tuples. Each TestShellThread pulls a list from this queue and runs
        those tests in order before grabbing the next available list.

        Shard the lists by directory. This helps ensure that tests that depend
        on each other (aka bad tests!) continue to run together as most
        cross-tests dependencies tend to occur within the same directory.

        Return:
          The Queue of lists of TestInfo objects.
        """

        if (self._options.experimental_fully_parallel or
            self._is_single_threaded()):
            filename_queue = Queue.Queue()
            for test_file in test_files:
                filename_queue.put(
                    ('.', [self._get_test_info_for_file(test_file)]))
            return filename_queue

        tests_by_dir = {}
        for test_file in test_files:
            directory = self._get_dir_for_test_file(test_file)
            tests_by_dir.setdefault(directory, [])
            tests_by_dir[directory].append(
                self._get_test_info_for_file(test_file))

        # Sort by the number of tests in the dir so that the ones with the
        # most tests get run first in order to maximize parallelization.
        # Number of tests is a good enough, but not perfect, approximation
        # of how long that set of tests will take to run. We can't just use
        # a PriorityQueue until we move # to Python 2.6.
        test_lists = []
        http_tests = None
        for directory in tests_by_dir:
            test_list = tests_by_dir[directory]
            # Keep the tests in alphabetical order.
            # TODO: Remove once tests are fixed so they can be run in any
            # order.
            test_list.reverse()
            test_list_tuple = (directory, test_list)
            if directory == 'LayoutTests' + os.sep + 'http':
                http_tests = test_list_tuple
            else:
                test_lists.append(test_list_tuple)
        test_lists.sort(lambda a, b: cmp(len(b[1]), len(a[1])))

        # Put the http tests first. There are only a couple hundred of them,
        # but each http test takes a very long time to run, so sorting by the
        # number of tests doesn't accurately capture how long they take to run.
        if http_tests:
            test_lists.insert(0, http_tests)

        filename_queue = Queue.Queue()
        for item in test_lists:
            filename_queue.put(item)
        return filename_queue

    def _get_test_shell_args(self, index):
        """Returns the tuple of arguments for tests and for test_shell."""
        shell_args = []
        test_args = test_type_base.TestArguments()
        png_path = None
        if not self._options.no_pixel_tests:
            png_path = os.path.join(self._options.results_directory,
                                    "png_result%s.png" % index)
            shell_args.append("--pixel-tests=" + png_path)
            test_args.png_path = png_path

        test_args.new_baseline = self._options.new_baseline

        test_args.show_sources = self._options.sources

        if self._options.startup_dialog:
            shell_args.append('--testshell-startup-dialog')

        if self._options.gp_fault_error_box:
            shell_args.append('--gp-fault-error-box')

        return test_args, png_path, shell_args

    def _contains_tests(self, subdir):
        for test_file in self._test_files_list:
            if test_file.find(subdir) >= 0:
                return True
        return False

    def _instantiate_test_shell_threads(self, test_files, result_summary):
        """Instantitates and starts the TestShellThread(s).

        Return:
          The list of threads.
        """
        filename_queue = self._get_test_file_queue(test_files)

        # Instantiate TestShellThreads and start them.
        threads = []
        for i in xrange(int(self._options.num_test_shells)):
            # Create separate TestTypes instances for each thread.
            test_types = []
            for t in self._test_types:
                test_types.append(t(self._port, self._options.platform,
                                    self._options.results_directory))

            test_args, png_path, shell_args = self._get_test_shell_args(i)
            thread = test_shell_thread.TestShellThread(self._port,
                                                       filename_queue,
                                                       self._result_queue,
                                                       test_types,
                                                       test_args,
                                                       png_path,
                                                       shell_args,
                                                       self._options)
            if self._is_single_threaded():
                thread.run_in_main_thread(self, result_summary)
            else:
                thread.start()
            threads.append(thread)

        return threads

    def _is_single_threaded(self):
        """Returns whether we should run all the tests in the main thread."""
        return int(self._options.num_test_shells) == 1

    def _run_tests(self, file_list, result_summary):
        """Runs the tests in the file_list.

        Return: A tuple (failures, thread_timings, test_timings,
            individual_test_timings)
            failures is a map from test to list of failure types
            thread_timings is a list of dicts with the total runtime
              of each thread with 'name', 'num_tests', 'total_time' properties
            test_timings is a list of timings for each sharded subdirectory
              of the form [time, directory_name, num_tests]
            individual_test_timings is a list of run times for each test
              in the form {filename:filename, test_run_time:test_run_time}
            result_summary: summary object to populate with the results
        """
        threads = self._instantiate_test_shell_threads(file_list,
                                                       result_summary)

        # Wait for the threads to finish and collect test failures.
        failures = {}
        test_timings = {}
        individual_test_timings = []
        thread_timings = []
        try:
            for thread in threads:
                while thread.isAlive():
                    # Let it timeout occasionally so it can notice a
                    # KeyboardInterrupt. Actually, the timeout doesn't
                    # really matter: apparently it suffices to not use
                    # an indefinite blocking join for it to
                    # be interruptible by KeyboardInterrupt.
                    thread.join(0.1)
                    self.update_summary(result_summary)
                thread_timings.append({'name': thread.getName(),
                                       'num_tests': thread.get_num_tests(),
                                       'total_time': thread.get_total_time()})
                test_timings.update(thread.get_directory_timing_stats())
                individual_test_timings.extend(
                    thread.get_individual_test_stats())
        except KeyboardInterrupt:
            for thread in threads:
                thread.cancel()
            self._port.stop_helper()
            raise
        for thread in threads:
            # Check whether a TestShellThread died before normal completion.
            exception_info = thread.get_exception_info()
            if exception_info is not None:
                # Re-raise the thread's exception here to make it clear that
                # testing was aborted. Otherwise, the tests that did not run
                # would be assumed to have passed.
                raise exception_info[0], exception_info[1], exception_info[2]

        # Make sure we pick up any remaining tests.
        self.update_summary(result_summary)
        return (thread_timings, test_timings, individual_test_timings)

    def run(self, result_summary):
        """Run all our tests on all our test files.

        For each test file, we run each test type. If there are any failures,
        we collect them for reporting.

        Args:
          result_summary: a summary object tracking the test results.

        Return:
          We return nonzero if there are regressions compared to the last run.
        """
        if not self._test_files:
            return 0
        start_time = time.time()

        # Start up any helper needed
        if not self._options.no_pixel_tests:
            self._port.start_helper()

        if self._contains_tests(self.HTTP_SUBDIR):
            self._port.start_http_server()

        if self._contains_tests(self.WEBSOCKET_SUBDIR):
            self._port.start_websocket_server()
            # self._websocket_secure_server.Start()

        thread_timings, test_timings, individual_test_timings = (
            self._run_tests(self._test_files_list, result_summary))

        # We exclude the crashes from the list of results to retry, because
        # we want to treat even a potentially flaky crash as an error.
        failures = self._get_failures(result_summary, include_crashes=False)
        retries = 0
        retry_summary = result_summary
        while (retries < self.NUM_RETRY_ON_UNEXPECTED_FAILURE and
               len(failures)):
            logging.debug("Retrying %d unexpected failure(s)" % len(failures))
            retries += 1
            retry_summary = ResultSummary(self._expectations, failures.keys())
            self._run_tests(failures.keys(), retry_summary)
            failures = self._get_failures(retry_summary, include_crashes=True)

        self._port.stop_helper()
        end_time = time.time()

        write = create_logging_writer(self._options, 'timing')
        self._print_timing_statistics(write, end_time - start_time,
                                    thread_timings, test_timings,
                                    individual_test_timings,
                                    result_summary)

        self._meter.update("")

        if self._options.verbose:
            # We write this block to stdout for compatibility with the
            # buildbot log parser, which only looks at stdout, not stderr :(
            write = lambda s: sys.stdout.write("%s\n" % s)
        else:
            write = create_logging_writer(self._options, 'actual')

        self._print_result_summary(write, result_summary)

        sys.stdout.flush()
        sys.stderr.flush()

        if (LOG_DETAILED_PROGRESS in self._options.log or
            (LOG_UNEXPECTED in self._options.log and
             result_summary.total != result_summary.expected)):
            print

        # This summary data gets written to stdout regardless of log level
        self._print_one_line_summary(result_summary.total,
                                  result_summary.expected)

        unexpected_results = self._summarize_unexpected_results(result_summary,
            retry_summary)
        self._print_unexpected_results(unexpected_results)

        # Write the same data to log files.
        self._write_json_files(unexpected_results, result_summary,
                             individual_test_timings)

        # Write the summary to disk (results.html) and maybe open the
        # test_shell to this file.
        wrote_results = self._write_results_html_file(result_summary)
        if not self._options.noshow_results and wrote_results:
            self._show_results_html_file()

        # Ignore flaky failures and unexpected passes so we don't turn the
        # bot red for those.
        return unexpected_results['num_regressions']

    def update_summary(self, result_summary):
        """Update the summary while running tests."""
        while True:
            try:
                (test, fail_list) = self._result_queue.get_nowait()
                result = test_failures.determine_result_type(fail_list)
                expected = self._expectations.matches_an_expected_result(test,
                                                                      result)
                result_summary.add(test, fail_list, result, expected)
                if (LOG_DETAILED_PROGRESS in self._options.log and
                    (self._options.experimental_fully_parallel or
                     self._is_single_threaded())):
                    self._display_detailed_progress(result_summary)
                else:
                    if not expected and LOG_UNEXPECTED in self._options.log:
                        self._print_unexpected_test_result(test, result)
                    self._display_one_line_progress(result_summary)
            except Queue.Empty:
                return

    def _display_one_line_progress(self, result_summary):
        """Displays the progress through the test run."""
        self._meter.update("Testing: %d ran as expected, %d didn't, %d left" %
            (result_summary.expected, result_summary.unexpected,
             result_summary.remaining))

    def _display_detailed_progress(self, result_summary):
        """Display detailed progress output where we print the directory name
        and one dot for each completed test. This is triggered by
        "--log detailed-progress"."""
        if self._current_test_number == len(self._test_files_list):
            return

        next_test = self._test_files_list[self._current_test_number]
        next_dir = os.path.dirname(
            self._port.relative_test_filename(next_test))
        if self._current_progress_str == "":
            self._current_progress_str = "%s: " % (next_dir)
            self._current_dir = next_dir

        while next_test in result_summary.results:
            if next_dir != self._current_dir:
                self._meter.write("%s\n" % (self._current_progress_str))
                self._current_progress_str = "%s: ." % (next_dir)
                self._current_dir = next_dir
            else:
                self._current_progress_str += "."

            if (next_test in result_summary.unexpected_results and
                LOG_UNEXPECTED in self._options.log):
                result = result_summary.unexpected_results[next_test]
                self._meter.write("%s\n" % self._current_progress_str)
                self._print_unexpected_test_result(next_test, result)
                self._current_progress_str = "%s: " % self._current_dir

            self._current_test_number += 1
            if self._current_test_number == len(self._test_files_list):
                break

            next_test = self._test_files_list[self._current_test_number]
            next_dir = os.path.dirname(
                self._port.relative_test_filename(next_test))

        if result_summary.remaining:
            remain_str = " (%d)" % (result_summary.remaining)
            self._meter.update("%s%s" %
                               (self._current_progress_str, remain_str))
        else:
            self._meter.write("%s\n" % (self._current_progress_str))

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
            if (result == test_expectations.PASS or
                result == test_expectations.CRASH and not include_crashes):
                continue
            failed_results[test] = result

        return failed_results

    def _summarize_unexpected_results(self, result_summary, retry_summary):
        """Summarize any unexpected results as a dict.

        TODO(dpranke): split this data structure into a separate class?

        Args:
          result_summary: summary object from initial test runs
          retry_summary: summary object from final test run of retried tests
        Returns:
          A dictionary containing a summary of the unexpected results from the
          run, with the following fields:
            'version': a version indicator (1 in this version)
            'fixable': # of fixable tests (NOW - PASS)
            'skipped': # of skipped tests (NOW & SKIPPED)
            'num_regressions': # of non-flaky failures
            'num_flaky': # of flaky failures
            'num_passes': # of unexpected passes
            'tests': a dict of tests -> {'expected': '...', 'actual': '...'}
        """
        results = {}
        results['version'] = 1

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
        for k, v in TestExpectationsFile.EXPECTATIONS.iteritems():
            keywords[v] = k.upper()

        tests = {}
        for filename, result in result_summary.unexpected_results.iteritems():
            # Note that if a test crashed in the original run, we ignore
            # whether or not it crashed when we retried it (if we retried it),
            # and always consider the result not flaky.
            test = self._port.relative_test_filename(filename)
            expected = self._expectations.get_expectations_string(filename)
            actual = [keywords[result]]

            if result == test_expectations.PASS:
                num_passes += 1
            elif result == test_expectations.CRASH:
                num_regressions += 1
            else:
                if filename not in retry_summary.unexpected_results:
                    actual.extend(
                        self._expectations.get_expectations_string(
                        filename).split(" "))
                    num_flaky += 1
                else:
                    retry_result = retry_summary.unexpected_results[filename]
                    if result != retry_result:
                        actual.append(keywords[retry_result])
                        num_flaky += 1
                    else:
                        num_regressions += 1

            tests[test] = {}
            tests[test]['expected'] = expected
            tests[test]['actual'] = " ".join(actual)

        results['tests'] = tests
        results['num_passes'] = num_passes
        results['num_flaky'] = num_flaky
        results['num_regressions'] = num_regressions

        return results

    def _write_json_files(self, unexpected_results, result_summary,
                        individual_test_timings):
        """Writes the results of the test run as JSON files into the results
        dir.

        There are three different files written into the results dir:
          unexpected_results.json: A short list of any unexpected results.
            This is used by the buildbots to display results.
          expectations.json: This is used by the flakiness dashboard.
          results.json: A full list of the results - used by the flakiness
            dashboard and the aggregate results dashboard.

        Args:
          unexpected_results: dict of unexpected results
          result_summary: full summary object
          individual_test_timings: list of test times (used by the flakiness
            dashboard).
        """
        logging.debug("Writing JSON files in %s." %
                      self._options.results_directory)
        unexpected_file = open(os.path.join(self._options.results_directory,
            "unexpected_results.json"), "w")
        unexpected_file.write(simplejson.dumps(unexpected_results,
                              sort_keys=True, indent=2))
        unexpected_file.close()

        # Write a json file of the test_expectations.txt file for the layout
        # tests dashboard.
        expectations_file = open(os.path.join(self._options.results_directory,
            "expectations.json"), "w")
        expectations_json = \
            self._expectations.get_expectations_json_for_all_platforms()
        expectations_file.write("ADD_EXPECTATIONS(" + expectations_json + ");")
        expectations_file.close()

        json_layout_results_generator.JSONLayoutResultsGenerator(
            self._port, self._options.builder_name, self._options.build_name,
            self._options.build_number, self._options.results_directory,
            BUILDER_BASE_URL, individual_test_timings,
            self._expectations, result_summary, self._test_files_list)

        logging.debug("Finished writing JSON files.")

    def _print_expected_results_of_type(self, write, result_summary,
                                        result_type, result_type_str):
        """Print the number of the tests in a given result class.

        Args:
          write: A callback to write info to (e.g., a LoggingWriter) or
             sys.stdout.write.
          result_summary - the object containing all the results to report on
          result_type - the particular result type to report in the summary.
          result_type_str - a string description of the result_type.
        """
        tests = self._expectations.get_tests_with_result_type(result_type)
        now = result_summary.tests_by_timeline[test_expectations.NOW]
        wontfix = result_summary.tests_by_timeline[test_expectations.WONTFIX]
        defer = result_summary.tests_by_timeline[test_expectations.DEFER]

        # We use a fancy format string in order to print the data out in a
        # nicely-aligned table.
        fmtstr = ("Expect: %%5d %%-8s (%%%dd now, %%%dd defer, %%%dd wontfix)"
                  % (self._num_digits(now), self._num_digits(defer),
                  self._num_digits(wontfix)))
        write(fmtstr % (len(tests), result_type_str, len(tests & now),
              len(tests & defer), len(tests & wontfix)))

    def _num_digits(self, num):
        """Returns the number of digits needed to represent the length of a
        sequence."""
        ndigits = 1
        if len(num):
            ndigits = int(math.log10(len(num))) + 1
        return ndigits

    def _print_timing_statistics(self, write, total_time, thread_timings,
                               directory_test_timings, individual_test_timings,
                               result_summary):
        """Record timing-specific information for the test run.

        Args:
          write: A callback to write info to (e.g., a LoggingWriter) or
              sys.stdout.write.
          total_time: total elapsed time (in seconds) for the test run
          thread_timings: wall clock time each thread ran for
          directory_test_timings: timing by directory
          individual_test_timings: timing by file
          result_summary: summary object for the test run
        """
        write("Test timing:")
        write("  %6.2f total testing time" % total_time)
        write("")
        write("Thread timing:")
        cuml_time = 0
        for t in thread_timings:
            write("    %10s: %5d tests, %6.2f secs" %
                  (t['name'], t['num_tests'], t['total_time']))
            cuml_time += t['total_time']
        write("   %6.2f cumulative, %6.2f optimal" %
              (cuml_time, cuml_time / int(self._options.num_test_shells)))
        write("")

        self._print_aggregate_test_statistics(write, individual_test_timings)
        self._print_individual_test_times(write, individual_test_timings,
                                          result_summary)
        self._print_directory_timings(write, directory_test_timings)

    def _print_aggregate_test_statistics(self, write, individual_test_timings):
        """Prints aggregate statistics (e.g. median, mean, etc.) for all tests.
        Args:
          write: A callback to write info to (e.g., a LoggingWriter) or
              sys.stdout.write.
          individual_test_timings: List of test_shell_thread.TestStats for all
              tests.
        """
        test_types = individual_test_timings[0].time_for_diffs.keys()
        times_for_test_shell = []
        times_for_diff_processing = []
        times_per_test_type = {}
        for test_type in test_types:
            times_per_test_type[test_type] = []

        for test_stats in individual_test_timings:
            times_for_test_shell.append(test_stats.test_run_time)
            times_for_diff_processing.append(
                test_stats.total_time_for_all_diffs)
            time_for_diffs = test_stats.time_for_diffs
            for test_type in test_types:
                times_per_test_type[test_type].append(
                    time_for_diffs[test_type])

        self._print_statistics_for_test_timings(write,
            "PER TEST TIME IN TESTSHELL (seconds):", times_for_test_shell)
        self._print_statistics_for_test_timings(write,
            "PER TEST DIFF PROCESSING TIMES (seconds):",
            times_for_diff_processing)
        for test_type in test_types:
            self._print_statistics_for_test_timings(write,
                "PER TEST TIMES BY TEST TYPE: %s" % test_type,
                times_per_test_type[test_type])

    def _print_individual_test_times(self, write, individual_test_timings,
                                  result_summary):
        """Prints the run times for slow, timeout and crash tests.
        Args:
          write: A callback to write info to (e.g., a LoggingWriter) or
              sys.stdout.write.
          individual_test_timings: List of test_shell_thread.TestStats for all
              tests.
          result_summary: summary object for test run
        """
        # Reverse-sort by the time spent in test_shell.
        individual_test_timings.sort(lambda a, b:
            cmp(b.test_run_time, a.test_run_time))

        num_printed = 0
        slow_tests = []
        timeout_or_crash_tests = []
        unexpected_slow_tests = []
        for test_tuple in individual_test_timings:
            filename = test_tuple.filename
            is_timeout_crash_or_slow = False
            if self._expectations.has_modifier(filename,
                                               test_expectations.SLOW):
                is_timeout_crash_or_slow = True
                slow_tests.append(test_tuple)

            if filename in result_summary.failures:
                result = result_summary.results[filename]
                if (result == test_expectations.TIMEOUT or
                    result == test_expectations.CRASH):
                    is_timeout_crash_or_slow = True
                    timeout_or_crash_tests.append(test_tuple)

            if (not is_timeout_crash_or_slow and
                num_printed < self._options.num_slow_tests_to_log):
                num_printed = num_printed + 1
                unexpected_slow_tests.append(test_tuple)

        write("")
        self._print_test_list_timing(write, "%s slowest tests that are not "
            "marked as SLOW and did not timeout/crash:" %
            self._options.num_slow_tests_to_log, unexpected_slow_tests)
        write("")
        self._print_test_list_timing(write, "Tests marked as SLOW:",
                                     slow_tests)
        write("")
        self._print_test_list_timing(write, "Tests that timed out or crashed:",
                                     timeout_or_crash_tests)
        write("")

    def _print_test_list_timing(self, write, title, test_list):
        """Print timing info for each test.

        Args:
          write: A callback to write info to (e.g., a LoggingWriter) or
              sys.stdout.write.
          title: section heading
          test_list: tests that fall in this section
        """
        write(title)
        for test_tuple in test_list:
            filename = test_tuple.filename[len(
                self._port.layout_tests_dir()) + 1:]
            filename = filename.replace('\\', '/')
            test_run_time = round(test_tuple.test_run_time, 1)
            write("  %s took %s seconds" % (filename, test_run_time))

    def _print_directory_timings(self, write, directory_test_timings):
        """Print timing info by directory for any directories that
        take > 10 seconds to run.

        Args:
          write: A callback to write info to (e.g., a LoggingWriter) or
              sys.stdout.write.
          directory_test_timing: time info for each directory
        """
        timings = []
        for directory in directory_test_timings:
            num_tests, time_for_directory = directory_test_timings[directory]
            timings.append((round(time_for_directory, 1), directory,
                            num_tests))
        timings.sort()

        write("Time to process slowest subdirectories:")
        min_seconds_to_print = 10
        for timing in timings:
            if timing[0] > min_seconds_to_print:
                write("  %s took %s seconds to run %s tests." % (timing[1],
                      timing[0], timing[2]))
        write("")

    def _print_statistics_for_test_timings(self, write, title, timings):
        """Prints the median, mean and standard deviation of the values in
        timings.

        Args:
          write: A callback to write info to (e.g., a LoggingWriter) or
              sys.stdout.write.
          title: Title for these timings.
          timings: A list of floats representing times.
        """
        write(title)
        timings.sort()

        num_tests = len(timings)
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
        write("  Median:          %6.3f" % median)
        write("  Mean:            %6.3f" % mean)
        write("  90th percentile: %6.3f" % percentile90)
        write("  99th percentile: %6.3f" % percentile99)
        write("  Standard dev:    %6.3f" % std_deviation)
        write("")

    def _print_result_summary(self, write, result_summary):
        """Print a short summary about how many tests passed.

        Args:
          write: A callback to write info to (e.g., a LoggingWriter) or
              sys.stdout.write.
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

        write("")
        write("=> Results: %d/%d tests passed (%.1f%%)" %
                     (passed, total, pct_passed))
        write("")
        self._print_result_summary_entry(write, result_summary,
            test_expectations.NOW, "Tests to be fixed for the current release")

        write("")
        self._print_result_summary_entry(write, result_summary,
            test_expectations.DEFER,
            "Tests we'll fix in the future if they fail (DEFER)")

        write("")
        self._print_result_summary_entry(write, result_summary,
            test_expectations.WONTFIX,
            "Tests that will only be fixed if they crash (WONTFIX)")

    def _print_result_summary_entry(self, write, result_summary, timeline,
                                    heading):
        """Print a summary block of results for a particular timeline of test.

        Args:
          write: A callback to write info to (e.g., a LoggingWriter) or
              sys.stdout.write.
          result_summary: summary to print results for
          timeline: the timeline to print results for (NOT, WONTFIX, etc.)
          heading: a textual description of the timeline
        """
        total = len(result_summary.tests_by_timeline[timeline])
        not_passing = (total -
           len(result_summary.tests_by_expectation[test_expectations.PASS] &
               result_summary.tests_by_timeline[timeline]))
        write("=> %s (%d):" % (heading, not_passing))

        for result in TestExpectationsFile.EXPECTATION_ORDER:
            if result == test_expectations.PASS:
                continue
            results = (result_summary.tests_by_expectation[result] &
                       result_summary.tests_by_timeline[timeline])
            desc = TestExpectationsFile.EXPECTATION_DESCRIPTIONS[result]
            if not_passing and len(results):
                pct = len(results) * 100.0 / not_passing
                write("  %5d %-24s (%4.1f%%)" % (len(results),
                      desc[len(results) != 1], pct))

    def _print_one_line_summary(self, total, expected):
        """Print a one-line summary of the test run to stdout.

        Args:
          total: total number of tests run
          expected: number of expected results
        """
        unexpected = total - expected
        if unexpected == 0:
            print "All %d tests ran as expected." % expected
        elif expected == 1:
            print "1 test ran as expected, %d didn't:" % unexpected
        else:
            print "%d tests ran as expected, %d didn't:" % (expected,
                unexpected)

    def _print_unexpected_results(self, unexpected_results):
        """Prints any unexpected results in a human-readable form to stdout."""
        passes = {}
        flaky = {}
        regressions = {}

        if len(unexpected_results['tests']):
            print ""

        for test, results in unexpected_results['tests'].iteritems():
            actual = results['actual'].split(" ")
            expected = results['expected'].split(" ")
            if actual == ['PASS']:
                if 'CRASH' in expected:
                    _add_to_dict_of_lists(passes,
                                          'Expected to crash, but passed',
                                          test)
                elif 'TIMEOUT' in expected:
                    _add_to_dict_of_lists(passes,
                                          'Expected to timeout, but passed',
                                           test)
                else:
                    _add_to_dict_of_lists(passes,
                                          'Expected to fail, but passed',
                                          test)
            elif len(actual) > 1:
                # We group flaky tests by the first actual result we got.
                _add_to_dict_of_lists(flaky, actual[0], test)
            else:
                _add_to_dict_of_lists(regressions, results['actual'], test)

        if len(passes):
            for key, tests in passes.iteritems():
                print "%s: (%d)" % (key, len(tests))
                tests.sort()
                for test in tests:
                    print "  %s" % test
                print

        if len(flaky):
            descriptions = TestExpectationsFile.EXPECTATION_DESCRIPTIONS
            for key, tests in flaky.iteritems():
                result = TestExpectationsFile.EXPECTATIONS[key.lower()]
                print "Unexpected flakiness: %s (%d)" % (
                  descriptions[result][1], len(tests))
                tests.sort()

                for test in tests:
                    result = unexpected_results['tests'][test]
                    actual = result['actual'].split(" ")
                    expected = result['expected'].split(" ")
                    result = TestExpectationsFile.EXPECTATIONS[key.lower()]
                    new_expectations_list = list(set(actual) | set(expected))
                    print "  %s = %s" % (test, " ".join(new_expectations_list))
                print

        if len(regressions):
            descriptions = TestExpectationsFile.EXPECTATION_DESCRIPTIONS
            for key, tests in regressions.iteritems():
                result = TestExpectationsFile.EXPECTATIONS[key.lower()]
                print "Regressions: Unexpected %s : (%d)" % (
                  descriptions[result][1], len(tests))
                tests.sort()
                for test in tests:
                    print "  %s = %s" % (test, key)
                print

        if len(unexpected_results['tests']) and self._options.verbose:
            print "-" * 78

    def _print_unexpected_test_result(self, test, result):
        """Prints one unexpected test result line."""
        desc = TestExpectationsFile.EXPECTATION_DESCRIPTIONS[result][0]
        self._meter.write("  %s -> unexpected %s\n" %
                          (self._port.relative_test_filename(test), desc))

    def _write_results_html_file(self, result_summary):
        """Write results.html which is a summary of tests that failed.

        Args:
          result_summary: a summary of the results :)

        Returns:
          True if any results were written (since expected failures may be
          omitted)
        """
        # test failures
        if self._options.full_results_html:
            test_files = result_summary.failures.keys()
        else:
            unexpected_failures = self._get_failures(result_summary,
                include_crashes=True)
            test_files = unexpected_failures.keys()
        if not len(test_files):
            return False

        out_filename = os.path.join(self._options.results_directory,
                                    "results.html")
        out_file = open(out_filename, 'w')
        # header
        if self._options.full_results_html:
            h2 = "Test Failures"
        else:
            h2 = "Unexpected Test Failures"
        out_file.write("<html><head><title>Layout Test Results (%(time)s)"
                       "</title></head><body><h2>%(h2)s (%(time)s)</h2>\n"
                       % {'h2': h2, 'time': time.asctime()})

        test_files.sort()
        for test_file in test_files:
            test_failures = result_summary.failures.get(test_file, [])
            out_file.write("<p><a href='%s'>%s</a><br />\n"
                           % (self._port.filename_to_uri(test_file),
                              self._port.relative_test_filename(test_file)))
            for failure in test_failures:
                out_file.write("&nbsp;&nbsp;%s<br/>"
                               % failure.result_html_output(
                                 self._port.relative_test_filename(test_file)))
            out_file.write("</p>\n")

        # footer
        out_file.write("</body></html>\n")
        return True

    def _show_results_html_file(self):
        """Launches the test shell open to the results.html page."""
        results_filename = os.path.join(self._options.results_directory,
                                        "results.html")
        self._port.show_results_html_file(results_filename)


def _add_to_dict_of_lists(dict, key, value):
    dict.setdefault(key, []).append(value)


def read_test_files(files):
    tests = []
    for file in files:
        for line in open(file):
            line = test_expectations.strip_comments(line)
            if line:
                tests.append(line)
    return tests


def create_logging_writer(options, log_option):
    """Returns a write() function that will write the string to logging.info()
    if comp was specified in --log or if --verbose is true. Otherwise the
    message is dropped.

    Args:
      options: list of command line options from optparse
      log_option: option to match in options.log in order for the messages
          to be logged (e.g., 'actual' or 'expected')
    """
    if options.verbose or log_option in options.log.split(","):
        return logging.info
    return lambda str: 1


def main(options, args):
    """Run the tests.  Will call sys.exit when complete.

    Args:
      options: a dictionary of command line options
      args: a list of sub directories or files to test
    """

    if options.sources:
        options.verbose = True

    # Set up our logging format.
    meter = metered_stream.MeteredStream(options.verbose, sys.stderr)
    log_fmt = '%(message)s'
    log_datefmt = '%y%m%d %H:%M:%S'
    log_level = logging.INFO
    if options.verbose:
        log_fmt = ('%(asctime)s %(filename)s:%(lineno)-4d %(levelname)s '
                  '%(message)s')
        log_level = logging.DEBUG
    logging.basicConfig(level=log_level, format=log_fmt, datefmt=log_datefmt,
                        stream=meter)

    if not options.target:
        if options.debug:
            options.target = "Debug"
        else:
            options.target = "Release"

    port_obj = port.get(options.platform, options)

    if not options.use_apache:
        options.use_apache = sys.platform in ('darwin', 'linux2')

    if options.results_directory.startswith("/"):
        # Assume it's an absolute path and normalize.
        options.results_directory = port_obj.get_absolute_path(
            options.results_directory)
    else:
        # If it's a relative path, make the output directory relative to
        # Debug or Release.
        options.results_directory = port_obj.results_directory()

    if options.clobber_old_results:
        # Just clobber the actual test results directories since the other
        # files in the results directory are explicitly used for cross-run
        # tracking.
        path = os.path.join(options.results_directory, 'LayoutTests')
        if os.path.exists(path):
            shutil.rmtree(path)

    if not options.num_test_shells:
        # TODO(ojan): Investigate perf/flakiness impact of using numcores + 1.
        options.num_test_shells = port_obj.num_cores()

    write = create_logging_writer(options, 'config')
    write("Running %s test_shells in parallel" % options.num_test_shells)

    if not options.time_out_ms:
        if options.target == "Debug":
            options.time_out_ms = str(2 * TestRunner.DEFAULT_TEST_TIMEOUT_MS)
        else:
            options.time_out_ms = str(TestRunner.DEFAULT_TEST_TIMEOUT_MS)

    options.slow_time_out_ms = str(5 * int(options.time_out_ms))
    write("Regular timeout: %s, slow test timeout: %s" %
          (options.time_out_ms, options.slow_time_out_ms))

    # Include all tests if none are specified.
    new_args = []
    for arg in args:
        if arg and arg != '':
            new_args.append(arg)

    paths = new_args
    if not paths:
        paths = []
    if options.test_list:
        paths += read_test_files(options.test_list)

    # Create the output directory if it doesn't already exist.
    port_obj.maybe_make_directory(options.results_directory)
    meter.update("Gathering files ...")

    test_runner = TestRunner(port_obj, options, meter)
    test_runner.gather_file_paths(paths)

    if options.lint_test_files:
        # Creating the expecations for each platform/target pair does all the
        # test list parsing and ensures it's correct syntax (e.g. no dupes).
        for platform in port_obj.test_platform_names():
            test_runner.parse_expectations(platform, is_debug_mode=True)
            test_runner.parse_expectations(platform, is_debug_mode=False)
        print ("If there are no fail messages, errors or exceptions, then the "
            "lint succeeded.")
        sys.exit(0)

    # Check that the system dependencies (themes, fonts, ...) are correct.
    if not options.nocheck_sys_deps:
        if not port_obj.check_sys_deps():
            sys.exit(1)

    write = create_logging_writer(options, "config")
    write("Using port '%s'" % port_obj.name())
    write("Placing test results in %s" % options.results_directory)
    if options.new_baseline:
        write("Placing new baselines in %s" % port_obj.baseline_path())
    write("Using %s build" % options.target)
    if options.no_pixel_tests:
        write("Not running pixel tests")
    write("")

    meter.update("Parsing expectations ...")
    test_runner.parse_expectations(port_obj.test_platform_name(),
                                   options.target == 'Debug')

    meter.update("Preparing tests ...")
    write = create_logging_writer(options, "expected")
    result_summary = test_runner.prepare_lists_and_print_output(write)

    port_obj.setup_test_run()

    test_runner.add_test_type(text_diff.TestTextDiff)
    if not options.no_pixel_tests:
        test_runner.add_test_type(image_diff.ImageDiff)
        if options.fuzzy_pixel_tests:
            test_runner.add_test_type(fuzzy_image_diff.FuzzyImageDiff)

    meter.update("Starting ...")
    has_new_failures = test_runner.run(result_summary)

    logging.debug("Exit status: %d" % has_new_failures)
    sys.exit(has_new_failures)


def parse_args(args=None):
    """Provides a default set of command line args.

    Returns a tuple of options, args from optparse"""
    option_parser = optparse.OptionParser()
    option_parser.add_option("", "--no-pixel-tests", action="store_true",
                             default=False,
                             help="disable pixel-to-pixel PNG comparisons")
    option_parser.add_option("", "--fuzzy-pixel-tests", action="store_true",
                             default=False,
                             help="Also use fuzzy matching to compare pixel "
                                  "test outputs.")
    option_parser.add_option("", "--results-directory",
                             default="layout-test-results",
                             help="Output results directory source dir,"
                                  " relative to Debug or Release")
    option_parser.add_option("", "--new-baseline", action="store_true",
                             default=False,
                             help="save all generated results as new baselines"
                                  " into the platform directory, overwriting "
                                  "whatever's already there.")
    option_parser.add_option("", "--noshow-results", action="store_true",
                             default=False, help="don't launch the test_shell"
                             " with results after the tests are done")
    option_parser.add_option("", "--full-results-html", action="store_true",
                             default=False, help="show all failures in "
                             "results.html, rather than only regressions")
    option_parser.add_option("", "--clobber-old-results", action="store_true",
                             default=False, help="Clobbers test results from "
                             "previous runs.")
    option_parser.add_option("", "--lint-test-files", action="store_true",
                             default=False, help="Makes sure the test files "
                             "parse for all configurations. Does not run any "
                             "tests.")
    option_parser.add_option("", "--force", action="store_true",
                             default=False,
                             help="Run all tests, even those marked SKIP "
                                  "in the test list")
    option_parser.add_option("", "--num-test-shells",
                             help="Number of testshells to run in parallel.")
    option_parser.add_option("", "--use-apache", action="store_true",
                             default=False,
                             help="Whether to use apache instead of lighttpd.")
    option_parser.add_option("", "--time-out-ms", default=None,
                             help="Set the timeout for each test")
    option_parser.add_option("", "--run-singly", action="store_true",
                             default=False,
                             help="run a separate test_shell for each test")
    option_parser.add_option("", "--debug", action="store_true", default=False,
                             help="use the debug binary instead of the release"
                                  " binary")
    option_parser.add_option("", "--num-slow-tests-to-log", default=50,
                             help="Number of slow tests whose timings "
                                  "to print.")
    option_parser.add_option("", "--platform",
                             help="Override the platform for expected results")
    option_parser.add_option("", "--target", default="",
                             help="Set the build target configuration "
                                  "(overrides --debug)")
    option_parser.add_option("", "--log", action="store",
                             default="detailed-progress,unexpected",
                             help="log various types of data. The param should"
                             " be a comma-separated list of values from: "
                             "actual,config," + LOG_DETAILED_PROGRESS +
                             ",expected,timing," + LOG_UNEXPECTED + " "
                             "(defaults to " +
                             "--log detailed-progress,unexpected)")
    option_parser.add_option("-v", "--verbose", action="store_true",
                             default=False, help="include debug-level logging")
    option_parser.add_option("", "--sources", action="store_true",
                             help="show expected result file path for each "
                                  "test (implies --verbose)")
    option_parser.add_option("", "--startup-dialog", action="store_true",
                             default=False,
                             help="create a dialog on test_shell.exe startup")
    option_parser.add_option("", "--gp-fault-error-box", action="store_true",
                             default=False,
                             help="enable Windows GP fault error box")
    option_parser.add_option("", "--wrapper",
                             help="wrapper command to insert before "
                                  "invocations of test_shell; option is split "
                                  "on whitespace before running. (Example: "
                                  "--wrapper='valgrind --smc-check=all')")
    option_parser.add_option("", "--test-list", action="append",
                             help="read list of tests to run from file",
                             metavar="FILE")
    option_parser.add_option("", "--nocheck-sys-deps", action="store_true",
                             default=False,
                             help="Don't check the system dependencies "
                                  "(themes)")
    option_parser.add_option("", "--randomize-order", action="store_true",
                             default=False,
                             help=("Run tests in random order (useful for "
                                   "tracking down corruption)"))
    option_parser.add_option("", "--run-chunk",
                             default=None,
                             help=("Run a specified chunk (n:l), the "
                                   "nth of len l, of the layout tests"))
    option_parser.add_option("", "--run-part",
                             default=None,
                             help=("Run a specified part (n:m), the nth of m"
                                   " parts, of the layout tests"))
    option_parser.add_option("", "--batch-size",
                             default=None,
                             help=("Run a the tests in batches (n), after "
                                   "every n tests, the test shell is "
                                   "relaunched."))
    option_parser.add_option("", "--builder-name",
                             default="DUMMY_BUILDER_NAME",
                             help=("The name of the builder shown on the "
                                   "waterfall running this script e.g. "
                                   "WebKit."))
    option_parser.add_option("", "--build-name",
                             default="DUMMY_BUILD_NAME",
                             help=("The name of the builder used in its path, "
                                   "e.g. webkit-rel."))
    option_parser.add_option("", "--build-number",
                             default="DUMMY_BUILD_NUMBER",
                             help=("The build number of the builder running"
                                   "this script."))
    option_parser.add_option("", "--experimental-fully-parallel",
                             action="store_true", default=False,
                             help="run all tests in parallel")
    return option_parser.parse_args(args)

if '__main__' == __name__:
    options, args = parse_args()
    main(options, args)
