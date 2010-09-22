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

"""Run layout tests.

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

from __future__ import with_statement

import codecs
import errno
import glob
import logging
import math
import optparse
import os
import platform
import Queue
import random
import re
import shutil
import signal
import sys
import time
import traceback

from layout_package import dump_render_tree_thread
from layout_package import json_layout_results_generator
from layout_package import printing
from layout_package import test_expectations
from layout_package import test_failures
from layout_package import test_results_uploader
from test_types import image_diff
from test_types import text_diff
from test_types import test_type_base

from webkitpy.common.system import user
from webkitpy.thirdparty import simplejson

import port

_log = logging.getLogger("webkitpy.layout_tests.run_webkit_tests")

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
        self._port = port
        self.uri = port.filename_to_uri(filename)
        self.timeout = timeout
        self._image_checksum = -1

    def image_hash(self):
        # Read the image_hash lazily to reduce startup time.
        # This class is accessed across threads, but only one thread should
        # ever be dealing with any given TestInfo so no locking is needed.
        #
        # Note that we use -1 to indicate that we haven't read the value,
        # because expected_checksum() returns a string or None.
        if self._image_checksum == -1:
            self._image_checksum = self._port.expected_checksum(self.filename)
        return self._image_checksum


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

    def add(self, result, expected):
        """Add a TestResult into the appropriate bin.

        Args:
          result: TestResult from dump_render_tree_thread.
          expected: whether the result was what we expected it to be.
        """

        self.tests_by_expectation[result.type].add(result.filename)
        self.results[result.filename] = result
        self.remaining -= 1
        if len(result.failures):
            self.failures[result.filename] = result.failures
        if expected:
            self.expected += 1
        else:
            self.unexpected_results[result.filename] = result.type
            self.unexpected += 1


def summarize_unexpected_results(port_obj, expectations, result_summary,
                                 retry_summary):
    """Summarize any unexpected results as a dict.

    FIXME: split this data structure into a separate class?

    Args:
        port_obj: interface to port-specific hooks
        expectations: test_expectations.TestExpectations object
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
        test = port_obj.relative_test_filename(filename)
        expected = expectations.get_expectations_string(filename)
        actual = [keywords[result]]

        if result == test_expectations.PASS:
            num_passes += 1
        elif result == test_expectations.CRASH:
            num_regressions += 1
        else:
            if filename not in retry_summary.unexpected_results:
                actual.extend(expectations.get_expectations_string(
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


class TestRunner:
    """A class for managing running a series of tests on a series of layout
    test files."""

    HTTP_SUBDIR = os.sep.join(['', 'http', ''])
    WEBSOCKET_SUBDIR = os.sep.join(['', 'websocket', ''])

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
        self._options = options
        self._printer = printer

        # disable wss server. need to install pyOpenSSL on buildbots.
        # self._websocket_secure_server = websocket_server.PyWebSocket(
        #        options.results_directory, use_tls=True, port=9323)

        # a list of TestType objects
        self._test_types = [text_diff.TestTextDiff]
        if options.pixel_tests:
            self._test_types.append(image_diff.ImageDiff)

        # a set of test files, and the same tests as a list
        self._test_files = set()
        self._test_files_list = None
        self._result_queue = Queue.Queue()
        self._retrying = False

    def collect_tests(self, args, last_unexpected_results):
        """Find all the files to test.

        Args:
          args: list of test arguments from the command line
          last_unexpected_results: list of unexpected results to retest, if any

        """
        paths = [arg for arg in args if arg and arg != '']
        paths += last_unexpected_results
        if self._options.test_list:
            paths += read_test_files(self._options.test_list)
        self._test_files = self._port.tests(paths)

    def lint(self):
        # Creating the expecations for each platform/configuration pair does
        # all the test list parsing and ensures it's correct syntax (e.g. no
        # dupes).
        for platform_name in self._port.test_platform_names():
            self.parse_expectations(platform_name, is_debug_mode=True)
            self.parse_expectations(platform_name, is_debug_mode=False)
        self._printer.write("")
        _log.info("If there are no fail messages, errors or exceptions, "
                  "then the lint succeeded.")
        return 0

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
            overrides_str = self._port.test_expectations_overrides()
            self._expectations = test_expectations.TestExpectations(
                self._port, test_files, expectations_str, test_platform_name,
                is_debug_mode, self._options.lint_test_files,
                overrides=overrides_str)
            return self._expectations
        except SyntaxError, err:
            if self._options.lint_test_files:
                print str(err)
            else:
                raise err

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
            if (self._options.run_chunk and
                (slice_end - slice_start < chunk_len)):
                extra = 1 + chunk_len - (slice_end - slice_start)
                extra_msg = ('   last chunk is partial, appending [0:%d]' %
                            extra)
                self._printer.print_expected(extra_msg)
                tests_run_msg += "\n" + extra_msg
                files.extend(test_files[0:extra])
            tests_run_filename = os.path.join(self._options.results_directory,
                                              "tests_run.txt")
            with codecs.open(tests_run_filename, "w", "utf-8") as file:
                file.write(tests_run_msg + "\n")

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
                self._options.configuration == 'Debug')

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
                result = dump_render_tree_thread.TestResult(test,
                    failures=[], test_run_time=0, total_time_for_all_diffs=0,
                    time_for_diffs=0)
                result.type = test_expectations.SKIP
                result_summary.add(result, expected=True)
        self._printer.print_expected('')

        return result_summary

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

    def _get_test_args(self, index):
        """Returns the tuple of arguments for tests and for DumpRenderTree."""
        test_args = test_type_base.TestArguments()
        test_args.png_path = None
        if self._options.pixel_tests:
            png_path = os.path.join(self._options.results_directory,
                                    "png_result%s.png" % index)
            test_args.png_path = png_path
        test_args.new_baseline = self._options.new_baseline
        test_args.reset_results = self._options.reset_results

        return test_args

    def _contains_tests(self, subdir):
        for test_file in self._test_files:
            if test_file.find(subdir) >= 0:
                return True
        return False

    def _instantiate_dump_render_tree_threads(self, test_files,
                                              result_summary):
        """Instantitates and starts the TestShellThread(s).

        Return:
          The list of threads.
        """
        filename_queue = self._get_test_file_queue(test_files)

        # Instantiate TestShellThreads and start them.
        threads = []
        for i in xrange(int(self._options.child_processes)):
            # Create separate TestTypes instances for each thread.
            test_types = []
            for test_type in self._test_types:
                test_types.append(test_type(self._port,
                                    self._options.results_directory))

            test_args = self._get_test_args(i)
            thread = dump_render_tree_thread.TestShellThread(self._port,
                self._options, filename_queue, self._result_queue,
                test_types, test_args)
            if self._is_single_threaded():
                thread.run_in_main_thread(self, result_summary)
            else:
                thread.start()
            threads.append(thread)

        return threads

    def _is_single_threaded(self):
        """Returns whether we should run all the tests in the main thread."""
        return int(self._options.child_processes) == 1

    def _run_tests(self, file_list, result_summary):
        """Runs the tests in the file_list.

        Return: A tuple (keyboard_interrupted, thread_timings, test_timings,
            individual_test_timings)
            keyboard_interrupted is whether someone typed Ctrl^C
            thread_timings is a list of dicts with the total runtime
              of each thread with 'name', 'num_tests', 'total_time' properties
            test_timings is a list of timings for each sharded subdirectory
              of the form [time, directory_name, num_tests]
            individual_test_timings is a list of run times for each test
              in the form {filename:filename, test_run_time:test_run_time}
            result_summary: summary object to populate with the results
        """
        # FIXME: We should use webkitpy.tool.grammar.pluralize here.
        plural = ""
        if not self._is_single_threaded():
            plural = "s"
        self._printer.print_update('Starting %s%s ...' %
                                   (self._port.driver_name(), plural))
        threads = self._instantiate_dump_render_tree_threads(file_list,
                                                             result_summary)
        self._printer.print_update("Starting testing ...")

        keyboard_interrupted = self._wait_for_threads_to_finish(threads,
                                                                result_summary)
        (thread_timings, test_timings, individual_test_timings) = \
            self._collect_timing_info(threads)

        return (keyboard_interrupted, thread_timings, test_timings,
                individual_test_timings)

    def _wait_for_threads_to_finish(self, threads, result_summary):
        keyboard_interrupted = False
        try:
            # Loop through all the threads waiting for them to finish.
            some_thread_is_alive = True
            while some_thread_is_alive:
                some_thread_is_alive = False
                t = time.time()
                for thread in threads:
                    exception_info = thread.exception_info()
                    if exception_info is not None:
                        # Re-raise the thread's exception here to make it
                        # clear that testing was aborted. Otherwise,
                        # the tests that did not run would be assumed
                        # to have passed.
                        raise exception_info[0], exception_info[1], exception_info[2]

                    if thread.isAlive():
                        some_thread_is_alive = True
                        next_timeout = thread.next_timeout()
                        if (next_timeout and t > next_timeout):
                            _log_wedged_thread(thread)
                            thread.clear_next_timeout()

                self.update_summary(result_summary)

                if some_thread_is_alive:
                    time.sleep(0.01)

        except KeyboardInterrupt:
            keyboard_interrupted = True
            for thread in threads:
                thread.cancel()

        return keyboard_interrupted

    def _collect_timing_info(self, threads):
        test_timings = {}
        individual_test_timings = []
        thread_timings = []

        for thread in threads:
            thread_timings.append({'name': thread.getName(),
                                   'num_tests': thread.get_num_tests(),
                                   'total_time': thread.get_total_time()})
            test_timings.update(thread.get_directory_timing_stats())
            individual_test_timings.extend(thread.get_test_results())

        return (thread_timings, test_timings, individual_test_timings)

    def needs_http(self):
        """Returns whether the test runner needs an HTTP server."""
        return self._contains_tests(self.HTTP_SUBDIR)

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
        self._port.maybe_make_directory(self._options.results_directory)

        self._port.setup_test_run()

        self._printer.print_update("Preparing tests ...")
        result_summary = self.prepare_lists_and_print_output()
        if not result_summary:
            return None

        if self.needs_http():
            self._printer.print_update('Starting HTTP server ...')
            self._port.start_http_server()

        if self._contains_tests(self.WEBSOCKET_SUBDIR):
            self._printer.print_update('Starting WebSocket server ...')
            self._port.start_websocket_server()
            # self._websocket_secure_server.Start()

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

        keyboard_interrupted, thread_timings, test_timings, \
            individual_test_timings = (
            self._run_tests(self._test_files_list, result_summary))

        # We exclude the crashes from the list of results to retry, because
        # we want to treat even a potentially flaky crash as an error.
        failures = self._get_failures(result_summary, include_crashes=False)
        retry_summary = result_summary
        while (len(failures) and self._options.retry_failures and
            not self._retrying and not keyboard_interrupted):
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

        unexpected_results = summarize_unexpected_results(self._port,
            self._expectations, result_summary, retry_summary)
        self._printer.print_unexpected_results(unexpected_results)

        if self._options.record_results:
            # Write the same data to log files.
            self._write_json_files(unexpected_results, result_summary,
                                   individual_test_timings)

            # Upload generated JSON files to appengine server.
            self._upload_json_files()

        # Write the summary to disk (results.html) and display it if requested.
        wrote_results = self._write_results_html_file(result_summary)
        if self._options.show_results and wrote_results:
            self._show_results_html_file()

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
        _log.debug("stopping http server")
        self._port.stop_http_server()
        _log.debug("stopping websocket server")
        self._port.stop_websocket_server()
        _log.debug("stopping helper")
        self._port.stop_helper()

    def update_summary(self, result_summary):
        """Update the summary and print results with any completed tests."""
        while True:
            try:
                result = self._result_queue.get_nowait()
            except Queue.Empty:
                return

            expected = self._expectations.matches_an_expected_result(
                result.filename, result.type, self._options.pixel_tests)
            result_summary.add(result, expected)
            exp_str = self._expectations.get_expectations_string(
                result.filename)
            got_str = self._expectations.expectation_to_string(result.type)
            self._printer.print_test_result(result, expected, exp_str, got_str)
            self._printer.print_progress(result_summary, self._retrying,
                                         self._test_files_list)

    def _clobber_old_results(self):
        # Just clobber the actual test results directories since the other
        # files in the results directory are explicitly used for cross-run
        # tracking.
        self._printer.print_update("Clobbering old results in %s" %
                                   self._options.results_directory)
        layout_tests_dir = self._port.layout_tests_dir()
        possible_dirs = self._port.test_dirs()
        for dirname in possible_dirs:
            if os.path.isdir(os.path.join(layout_tests_dir, dirname)):
                shutil.rmtree(os.path.join(self._options.results_directory,
                                           dirname),
                              ignore_errors=True)

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
        results_directory = self._options.results_directory
        _log.debug("Writing JSON files in %s." % results_directory)
        unexpected_json_path = os.path.join(results_directory, "unexpected_results.json")
        with codecs.open(unexpected_json_path, "w", "utf-8") as file:
            simplejson.dump(unexpected_results, file, sort_keys=True, indent=2)

        # Write a json file of the test_expectations.txt file for the layout
        # tests dashboard.
        expectations_path = os.path.join(results_directory, "expectations.json")
        expectations_json = \
            self._expectations.get_expectations_json_for_all_platforms()
        with codecs.open(expectations_path, "w", "utf-8") as file:
            file.write(u"ADD_EXPECTATIONS(%s);" % expectations_json)

        json_layout_results_generator.JSONLayoutResultsGenerator(
            self._port, self._options.builder_name, self._options.build_name,
            self._options.build_number, self._options.results_directory,
            BUILDER_BASE_URL, individual_test_timings,
            self._expectations, result_summary, self._test_files_list,
            not self._options.upload_full_results,
            self._options.test_results_server)

        _log.debug("Finished writing JSON files.")

    def _upload_json_files(self):
        if not self._options.test_results_server:
            return

        _log.info("Uploading JSON files for builder: %s",
                   self._options.builder_name)

        attrs = [("builder", self._options.builder_name)]
        json_files = ["expectations.json"]
        if self._options.upload_full_results:
            json_files.append("results.json")
        else:
            json_files.append("incremental_results.json")

        files = [(file, os.path.join(self._options.results_directory, file))
            for file in json_files]

        uploader = test_results_uploader.TestResultsUploader(
            self._options.test_results_server)
        try:
            # Set uploading timeout in case appengine server is having problem.
            # 120 seconds are more than enough to upload test results.
            uploader.upload(attrs, files, 120)
        except Exception, err:
            _log.error("Upload failed: %s" % err)
            return

        _log.info("JSON files uploaded.")

    def _print_config(self):
        """Prints the configuration for the test run."""
        p = self._printer
        p.print_config("Using port '%s'" % self._port.name())
        p.print_config("Placing test results in %s" %
                       self._options.results_directory)
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

        if self._is_single_threaded():
            p.print_config("Running one %s" % self._port.driver_name())
        else:
            p.print_config("Running %s %ss in parallel" %
                           (self._options.child_processes,
                            self._port.driver_name()))
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
        defer = result_summary.tests_by_timeline[test_expectations.DEFER]

        # We use a fancy format string in order to print the data out in a
        # nicely-aligned table.
        fmtstr = ("Expect: %%5d %%-8s (%%%dd now, %%%dd defer, %%%dd wontfix)"
                  % (self._num_digits(now), self._num_digits(defer),
                  self._num_digits(wontfix)))
        self._printer.print_expected(fmtstr %
            (len(tests), result_type_str, len(tests & now),
             len(tests & defer), len(tests & wontfix)))

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
          individual_test_timings: List of dump_render_tree_thread.TestStats
              for all tests.
        """
        test_types = []  # Unit tests don't actually produce any timings.
        if individual_test_timings:
            test_types = individual_test_timings[0].time_for_diffs.keys()
        times_for_dump_render_tree = []
        times_for_diff_processing = []
        times_per_test_type = {}
        for test_type in test_types:
            times_per_test_type[test_type] = []

        for test_stats in individual_test_timings:
            times_for_dump_render_tree.append(test_stats.test_run_time)
            times_for_diff_processing.append(
                test_stats.total_time_for_all_diffs)
            time_for_diffs = test_stats.time_for_diffs
            for test_type in test_types:
                times_per_test_type[test_type].append(
                    time_for_diffs[test_type])

        self._print_statistics_for_test_timings(
            "PER TEST TIME IN TESTSHELL (seconds):",
            times_for_dump_render_tree)
        self._print_statistics_for_test_timings(
            "PER TEST DIFF PROCESSING TIMES (seconds):",
            times_for_diff_processing)
        for test_type in test_types:
            self._print_statistics_for_test_timings(
                "PER TEST TIMES BY TEST TYPE: %s" % test_type,
                times_per_test_type[test_type])

    def _print_individual_test_times(self, individual_test_timings,
                                  result_summary):
        """Prints the run times for slow, timeout and crash tests.
        Args:
          individual_test_timings: List of dump_render_tree_thread.TestStats
              for all tests.
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
            if self._expectations.has_modifier(filename,
                                               test_expectations.SLOW):
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
            test_expectations.NOW, "Tests to be fixed for the current release")

        self._printer.print_actual("")
        self._print_result_summary_entry(result_summary,
            test_expectations.DEFER,
            "Tests we'll fix in the future if they fail (DEFER)")

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

    def _results_html(self, test_files, failures, title="Test Failures", override_time=None):
        """
        test_files = a list of file paths
        failures = dictionary mapping test paths to failure objects
        title = title printed at top of test
        override_time = current time (used by unit tests)
        """
        page = """<html>
  <head>
    <title>Layout Test Results (%(time)s)</title>
  </head>
  <body>
    <h2>%(title)s (%(time)s)</h2>
        """ % {'title': title, 'time': override_time or time.asctime()}

        for test_file in sorted(test_files):
            test_name = self._port.relative_test_filename(test_file)
            test_url = self._port.filename_to_uri(test_file)
            page += u"<p><a href='%s'>%s</a><br />\n" % (test_url, test_name)
            test_failures = failures.get(test_file, [])
            for failure in test_failures:
                page += u"&nbsp;&nbsp;%s<br/>" % failure.result_html_output(test_name)
            page += "</p>\n"
        page += "</body></html>\n"
        return page

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
            results_title = "Test Failures"
            test_files = result_summary.failures.keys()
        else:
            results_title = "Unexpected Test Failures"
            unexpected_failures = self._get_failures(result_summary,
                include_crashes=True)
            test_files = unexpected_failures.keys()
        if not len(test_files):
            return False

        out_filename = os.path.join(self._options.results_directory,
                                    "results.html")
        with codecs.open(out_filename, "w", "utf-8") as results_file:
            html = self._results_html(test_files, result_summary.failures, results_title)
            results_file.write(html)

        return True

    def _show_results_html_file(self):
        """Shows the results.html page."""
        results_filename = os.path.join(self._options.results_directory,
                                        "results.html")
        self._port.show_results_html_file(results_filename)


def read_test_files(files):
    tests = []
    for file in files:
        try:
            with codecs.open(file, 'r', 'utf-8') as file_contents:
                # FIXME: This could be cleaner using a list comprehension.
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


def run(port, options, args, regular_output=sys.stderr,
        buildbot_output=sys.stdout):
    """Run the tests.

    Args:
      port: Port object for port-specific behavior
      options: a dictionary of command line options
      args: a list of sub directories or files to test
      regular_output: a stream-like object that we can send logging/debug
          output to
      buildbot_output: a stream-like object that we can write all output that
          is intended to be parsed by the buildbot to
    Returns:
      the number of unexpected results that occurred, or -1 if there is an
          error.

    """
    _set_up_derived_options(port, options)

    printer = printing.Printer(port, options, regular_output, buildbot_output,
        int(options.child_processes), options.experimental_fully_parallel)
    if options.help_printing:
        printer.help_printing()
        printer.cleanup()
        return 0

    last_unexpected_results = _gather_unexpected_results(options)
    if options.print_last_failures:
        printer.write("\n".join(last_unexpected_results) + "\n")
        printer.cleanup()
        return 0

    # We wrap any parts of the run that are slow or likely to raise exceptions
    # in a try/finally to ensure that we clean up the logging configuration.
    num_unexpected_results = -1
    try:
        test_runner = TestRunner(port, options, printer)
        test_runner._print_config()

        printer.print_update("Collecting tests ...")
        try:
            test_runner.collect_tests(args, last_unexpected_results)
        except IOError, e:
            if e.errno == errno.ENOENT:
                return -1
            raise

        printer.print_update("Parsing expectations ...")
        if options.lint_test_files:
            return test_runner.lint()
        test_runner.parse_expectations(port.test_platform_name(),
                                       options.configuration == 'Debug')

        printer.print_update("Checking build ...")
        if not port.check_build(test_runner.needs_http()):
            return -1

        result_summary = test_runner.set_up_run()
        if result_summary:
            num_unexpected_results = test_runner.run(result_summary)
            test_runner.clean_up_run()
            _log.debug("Testing completed, Exit status: %d" %
                       num_unexpected_results)
    finally:
        printer.cleanup()

    return num_unexpected_results


def _set_up_derived_options(port_obj, options):
    """Sets the options values that depend on other options values."""

    if not options.child_processes:
        # FIXME: Investigate perf/flakiness impact of using cpu_count + 1.
        options.child_processes = str(port_obj.default_child_processes())

    if not options.configuration:
        options.configuration = port_obj.default_configuration()

    if options.pixel_tests is None:
        options.pixel_tests = True

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

    if not options.time_out_ms:
        if options.configuration == "Debug":
            options.time_out_ms = str(2 * TestRunner.DEFAULT_TEST_TIMEOUT_MS)
        else:
            options.time_out_ms = str(TestRunner.DEFAULT_TEST_TIMEOUT_MS)

    options.slow_time_out_ms = str(5 * int(options.time_out_ms))


def _gather_unexpected_results(options):
    """Returns the unexpected results from the previous run, if any."""
    last_unexpected_results = []
    if options.print_last_failures or options.retest_last_failures:
        unexpected_results_filename = os.path.join(
        options.results_directory, "unexpected_results.json")
        with codecs.open(unexpected_results_filename, "r", "utf-8") as file:
            results = simplejson.load(file)
        last_unexpected_results = results['tests'].keys()
    return last_unexpected_results


def _compat_shim_callback(option, opt_str, value, parser):
    print "Ignoring unsupported option: %s" % opt_str


def _compat_shim_option(option_name, **kwargs):
    return optparse.make_option(option_name, action="callback",
        callback=_compat_shim_callback,
        help="Ignored, for old-run-webkit-tests compat only.", **kwargs)


def parse_args(args=None):
    """Provides a default set of command line args.

    Returns a tuple of options, args from optparse"""

    # FIXME: All of these options should be stored closer to the code which
    # FIXME: actually uses them. configuration_options should move
    # FIXME: to WebKitPort and be shared across all scripts.
    configuration_options = [
        optparse.make_option("-t", "--target", dest="configuration",
                             help="(DEPRECATED)"),
        # FIXME: --help should display which configuration is default.
        optparse.make_option('--debug', action='store_const', const='Debug',
                             dest="configuration",
                             help='Set the configuration to Debug'),
        optparse.make_option('--release', action='store_const',
                             const='Release', dest="configuration",
                             help='Set the configuration to Release'),
        # old-run-webkit-tests also accepts -c, --configuration CONFIGURATION.
    ]

    print_options = printing.print_options()

    # FIXME: These options should move onto the ChromiumPort.
    chromium_options = [
        optparse.make_option("--chromium", action="store_true", default=False,
            help="use the Chromium port"),
        optparse.make_option("--startup-dialog", action="store_true",
            default=False, help="create a dialog on DumpRenderTree startup"),
        optparse.make_option("--gp-fault-error-box", action="store_true",
            default=False, help="enable Windows GP fault error box"),
        optparse.make_option("--nocheck-sys-deps", action="store_true",
            default=False,
            help="Don't check the system dependencies (themes)"),
        optparse.make_option("--use-drt", action="store_true",
            default=False,
            help="Use DumpRenderTree instead of test_shell"),
        optparse.make_option("--accelerated-compositing",
            action="store_true",
            help="Use hardware-accelated compositing for rendering"),
        optparse.make_option("--no-accelerated-compositing",
            action="store_false",
            dest="accelerated_compositing",
            help="Don't use hardware-accelerated compositing for rendering"),
        optparse.make_option("--accelerated-2d-canvas",
            action="store_true",
            help="Use hardware-accelerated 2D Canvas calls"),
        optparse.make_option("--no-accelerated-2d-canvas",
            action="store_false",
            dest="accelerated_2d_canvas",
            help="Don't use hardware-accelerated 2D Canvas calls"),
    ]

    # Missing Mac-specific old-run-webkit-tests options:
    # FIXME: Need: -g, --guard for guard malloc support on Mac.
    # FIXME: Need: -l --leaks    Enable leaks checking.
    # FIXME: Need: --sample-on-timeout Run sample on timeout

    old_run_webkit_tests_compat = [
        # NRWT doesn't generate results by default anyway.
        _compat_shim_option("--no-new-test-results"),
        # NRWT doesn't sample on timeout yet anyway.
        _compat_shim_option("--no-sample-on-timeout"),
        # FIXME: NRWT needs to support remote links eventually.
        _compat_shim_option("--use-remote-links-to-tests"),
        # FIXME: NRWT doesn't need this option as much since failures are
        # designed to be cheap.  We eventually plan to add this support.
        _compat_shim_option("--exit-after-n-failures", nargs=1, type="int"),
    ]

    results_options = [
        # NEED for bots: --use-remote-links-to-tests Link to test files
        # within the SVN repository in the results.
        optparse.make_option("-p", "--pixel-tests", action="store_true",
            dest="pixel_tests", help="Enable pixel-to-pixel PNG comparisons"),
        optparse.make_option("--no-pixel-tests", action="store_false",
            dest="pixel_tests", help="Disable pixel-to-pixel PNG comparisons"),
        # old-run-webkit-tests allows a specific tolerance: --tolerance t
        # Ignore image differences less than this percentage (default: 0.1)
        optparse.make_option("--results-directory",
            default="layout-test-results",
            help="Output results directory source dir, relative to Debug or "
                 "Release"),
        optparse.make_option("--new-baseline", action="store_true",
            default=False, help="Save all generated results as new baselines "
                 "into the platform directory, overwriting whatever's "
                 "already there."),
        optparse.make_option("--reset-results", action="store_true",
            default=False, help="Reset any existing baselines to the "
                 "generated results"),
        optparse.make_option("--no-show-results", action="store_false",
            default=True, dest="show_results",
            help="Don't launch a browser with results after the tests "
                 "are done"),
        # FIXME: We should have a helper function to do this sort of
        # deprectated mapping and automatically log, etc.
        optparse.make_option("--noshow-results", action="store_false",
            dest="show_results",
            help="Deprecated, same as --no-show-results."),
        optparse.make_option("--no-launch-safari", action="store_false",
            dest="show_results",
            help="old-run-webkit-tests compat, same as --noshow-results."),
        # old-run-webkit-tests:
        # --[no-]launch-safari    Launch (or do not launch) Safari to display
        #                         test results (default: launch)
        optparse.make_option("--full-results-html", action="store_true",
            default=False,
            help="Show all failures in results.html, rather than only "
                 "regressions"),
        optparse.make_option("--clobber-old-results", action="store_true",
            default=False, help="Clobbers test results from previous runs."),
        optparse.make_option("--platform",
            help="Override the platform for expected results"),
        optparse.make_option("--no-record-results", action="store_false",
            default=True, dest="record_results",
            help="Don't record the results."),
        # old-run-webkit-tests also has HTTP toggle options:
        # --[no-]http                     Run (or do not run) http tests
        #                                 (default: run)
        # --[no-]wait-for-httpd           Wait for httpd if some other test
        #                                 session is using it already (same
        #                                 as WEBKIT_WAIT_FOR_HTTPD=1).
        #                                 (default: 0)
    ]

    test_options = [
        optparse.make_option("--build", dest="build",
            action="store_true", default=True,
            help="Check to ensure the DumpRenderTree build is up-to-date "
                 "(default)."),
        optparse.make_option("--no-build", dest="build",
            action="store_false", help="Don't check to see if the "
                                       "DumpRenderTree build is up-to-date."),
        # old-run-webkit-tests has --valgrind instead of wrapper.
        optparse.make_option("--wrapper",
            help="wrapper command to insert before invocations of "
                 "DumpRenderTree; option is split on whitespace before "
                 "running. (Example: --wrapper='valgrind --smc-check=all')"),
        # old-run-webkit-tests:
        # -i|--ignore-tests               Comma-separated list of directories
        #                                 or tests to ignore
        optparse.make_option("--test-list", action="append",
            help="read list of tests to run from file", metavar="FILE"),
        # old-run-webkit-tests uses --skipped==[default|ignore|only]
        # instead of --force:
        optparse.make_option("--force", action="store_true", default=False,
            help="Run all tests, even those marked SKIP in the test list"),
        optparse.make_option("--use-apache", action="store_true",
            default=False, help="Whether to use apache instead of lighttpd."),
        optparse.make_option("--time-out-ms",
            help="Set the timeout for each test"),
        # old-run-webkit-tests calls --randomize-order --random:
        optparse.make_option("--randomize-order", action="store_true",
            default=False, help=("Run tests in random order (useful "
                                "for tracking down corruption)")),
        optparse.make_option("--run-chunk",
            help=("Run a specified chunk (n:l), the nth of len l, "
                 "of the layout tests")),
        optparse.make_option("--run-part", help=("Run a specified part (n:m), "
                  "the nth of m parts, of the layout tests")),
        # old-run-webkit-tests calls --batch-size: --nthly n
        #   Restart DumpRenderTree every n tests (default: 1000)
        optparse.make_option("--batch-size",
            help=("Run a the tests in batches (n), after every n tests, "
                  "DumpRenderTree is relaunched."), type="int", default=0),
        # old-run-webkit-tests calls --run-singly: -1|--singly
        # Isolate each test case run (implies --nthly 1 --verbose)
        optparse.make_option("--run-singly", action="store_true",
            default=False, help="run a separate DumpRenderTree for each test"),
        optparse.make_option("--child-processes",
            help="Number of DumpRenderTrees to run in parallel."),
        # FIXME: Display default number of child processes that will run.
        optparse.make_option("--experimental-fully-parallel",
            action="store_true", default=False,
            help="run all tests in parallel"),
        # FIXME: Need --exit-after-n-failures N
        #      Exit after the first N failures instead of running all tests
        # FIXME: Need --exit-after-n-crashes N
        #      Exit after the first N crashes instead of running all tests
        # FIXME: consider: --iterations n
        #      Number of times to run the set of tests (e.g. ABCABCABC)
        optparse.make_option("--print-last-failures", action="store_true",
            default=False, help="Print the tests in the last run that "
            "had unexpected failures (or passes)."),
        optparse.make_option("--retest-last-failures", action="store_true",
            default=False, help="re-test the tests in the last run that "
            "had unexpected failures (or passes)."),
        optparse.make_option("--retry-failures", action="store_true",
            default=True,
            help="Re-try any tests that produce unexpected results (default)"),
        optparse.make_option("--no-retry-failures", action="store_false",
            dest="retry_failures",
            help="Don't re-try any tests that produce unexpected results."),
    ]

    misc_options = [
        optparse.make_option("--lint-test-files", action="store_true",
        default=False, help=("Makes sure the test files parse for all "
                            "configurations. Does not run any tests.")),
    ]

    # FIXME: Move these into json_results_generator.py
    results_json_options = [
        optparse.make_option("--builder-name", default="DUMMY_BUILDER_NAME",
            help=("The name of the builder shown on the waterfall running "
                  "this script e.g. WebKit.")),
        optparse.make_option("--build-name", default="DUMMY_BUILD_NAME",
            help=("The name of the builder used in its path, e.g. "
                  "webkit-rel.")),
        optparse.make_option("--build-number", default="DUMMY_BUILD_NUMBER",
            help=("The build number of the builder running this script.")),
        optparse.make_option("--test-results-server", default="",
            help=("If specified, upload results json files to this appengine "
                  "server.")),
        optparse.make_option("--upload-full-results",
            action="store_true",
            default=False,
            help="If true, upload full json results to server."),
    ]

    option_list = (configuration_options + print_options +
                   chromium_options + results_options + test_options +
                   misc_options + results_json_options +
                   old_run_webkit_tests_compat)
    option_parser = optparse.OptionParser(option_list=option_list)

    options, args = option_parser.parse_args(args)

    return options, args


def _log_wedged_thread(thread):
    """Log information about the given thread state."""
    id = thread.id()
    stack = dump_render_tree_thread.find_thread_stack(id)
    assert(stack is not None)
    _log.error("")
    _log.error("thread %s (%d) is wedged" % (thread.getName(), id))
    dump_render_tree_thread.log_stack(stack)
    _log.error("")


def main():
    options, args = parse_args()
    port_obj = port.get(options.platform, options)
    return run(port_obj, options, args)

if '__main__' == __name__:
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        # this mirrors what the shell normally does
        sys.exit(signal.SIGINT + 128)
