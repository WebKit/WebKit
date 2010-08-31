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

"""A Thread object for running DumpRenderTree and processing URLs from a
shared queue.

Each thread runs a separate instance of the DumpRenderTree binary and validates
the output.  When there are no more URLs to process in the shared queue, the
thread exits.
"""

from __future__ import with_statement

import codecs
import copy
import logging
import os
import Queue
import signal
import sys
import thread
import threading
import time

import test_failures

_log = logging.getLogger("webkitpy.layout_tests.layout_package."
                         "dump_render_tree_thread")


def _process_output(port, test_info, test_types, test_args, configuration,
                    output_dir, crash, timeout, test_run_time, actual_checksum,
                    output, error):
    """Receives the output from a DumpRenderTree process, subjects it to a
    number of tests, and returns a list of failure types the test produced.

    Args:
      port: port-specific hooks
      proc: an active DumpRenderTree process
      test_info: Object containing the test filename, uri and timeout
      test_types: list of test types to subject the output to
      test_args: arguments to be passed to each test
      configuration: Debug or Release
      output_dir: directory to put crash stack traces into

    Returns: a TestResult object
    """
    failures = []

    # Some test args, such as the image hash, may be added or changed on a
    # test-by-test basis.
    local_test_args = copy.copy(test_args)

    local_test_args.hash = actual_checksum

    if crash:
        failures.append(test_failures.FailureCrash())
    if timeout:
        failures.append(test_failures.FailureTimeout())

    if crash:
        _log.debug("Stacktrace for %s:\n%s" % (test_info.filename, error))
        # Strip off "file://" since RelativeTestFilename expects
        # filesystem paths.
        filename = os.path.join(output_dir, port.relative_test_filename(
                                test_info.filename))
        filename = os.path.splitext(filename)[0] + "-stack.txt"
        port.maybe_make_directory(os.path.split(filename)[0])
        with codecs.open(filename, "wb", "utf-8") as file:
            file.write(error)
    elif error:
        _log.debug("Previous test output stderr lines:\n%s" % error)

    # Check the output and save the results.
    start_time = time.time()
    time_for_diffs = {}
    for test_type in test_types:
        start_diff_time = time.time()
        new_failures = test_type.compare_output(port, test_info.filename,
                                                output, local_test_args,
                                                configuration)
        # Don't add any more failures if we already have a crash, so we don't
        # double-report those tests. We do double-report for timeouts since
        # we still want to see the text and image output.
        if not crash:
            failures.extend(new_failures)
        time_for_diffs[test_type.__class__.__name__] = (
            time.time() - start_diff_time)

    total_time_for_all_diffs = time.time() - start_diff_time
    return TestResult(test_info.filename, failures, test_run_time,
                      total_time_for_all_diffs, time_for_diffs)


def _pad_timeout(timeout):
    """Returns a safe multiple of the per-test timeout value to use
    to detect hung test threads.

    """
    # When we're running one test per DumpRenderTree process, we can
    # enforce a hard timeout.  The DumpRenderTree watchdog uses 2.5x
    # the timeout; we want to be larger than that.
    return timeout * 3


def _milliseconds_to_seconds(msecs):
    return float(msecs) / 1000.0


class TestResult(object):

    def __init__(self, filename, failures, test_run_time,
                 total_time_for_all_diffs, time_for_diffs):
        self.failures = failures
        self.filename = filename
        self.test_run_time = test_run_time
        self.time_for_diffs = time_for_diffs
        self.total_time_for_all_diffs = total_time_for_all_diffs
        self.type = test_failures.determine_result_type(failures)


class SingleTestThread(threading.Thread):
    """Thread wrapper for running a single test file."""

    def __init__(self, port, image_path, shell_args, test_info,
        test_types, test_args, configuration, output_dir):
        """
        Args:
          port: object implementing port-specific hooks
          test_info: Object containing the test filename, uri and timeout
          output_dir: Directory to put crash stacks into.
          See TestShellThread for documentation of the remaining arguments.
        """

        threading.Thread.__init__(self)
        self._port = port
        self._image_path = image_path
        self._shell_args = shell_args
        self._test_info = test_info
        self._test_types = test_types
        self._test_args = test_args
        self._configuration = configuration
        self._output_dir = output_dir

    def run(self):
        self._covered_run()

    def _covered_run(self):
        # FIXME: this is a separate routine to work around a bug
        # in coverage: see http://bitbucket.org/ned/coveragepy/issue/85.
        test_info = self._test_info
        driver = self._port.create_driver(self._image_path, self._shell_args)
        driver.start()
        start = time.time()
        crash, timeout, actual_checksum, output, error = \
            driver.run_test(test_info.uri.strip(), test_info.timeout,
                            test_info.image_hash())
        end = time.time()
        self._test_result = _process_output(self._port,
            test_info, self._test_types, self._test_args,
            self._configuration, self._output_dir, crash, timeout, end - start,
            actual_checksum, output, error)
        driver.stop()

    def get_test_result(self):
        return self._test_result


class WatchableThread(threading.Thread):
    """This class abstracts an interface used by
    run_webkit_tests.TestRunner._wait_for_threads_to_finish for thread
    management."""
    def __init__(self):
        threading.Thread.__init__(self)
        self._canceled = False
        self._exception_info = None
        self._next_timeout = None
        self._thread_id = None

    def cancel(self):
        """Set a flag telling this thread to quit."""
        self._canceled = True

    def clear_next_timeout(self):
        """Mark a flag telling this thread to stop setting timeouts."""
        self._timeout = 0

    def exception_info(self):
        """If run() terminated on an uncaught exception, return it here
        ((type, value, traceback) tuple).
        Returns None if run() terminated normally. Meant to be called after
        joining this thread."""
        return self._exception_info

    def id(self):
        """Return a thread identifier."""
        return self._thread_id

    def next_timeout(self):
        """Return the time the test is supposed to finish by."""
        return self._next_timeout


class TestShellThread(WatchableThread):
    def __init__(self, port, filename_list_queue, result_queue,
                 test_types, test_args, image_path, shell_args, options):
        """Initialize all the local state for this DumpRenderTree thread.

        Args:
          port: interface to port-specific hooks
          filename_list_queue: A thread safe Queue class that contains lists
              of tuples of (filename, uri) pairs.
          result_queue: A thread safe Queue class that will contain tuples of
              (test, failure lists) for the test results.
          test_types: A list of TestType objects to run the test output
              against.
          test_args: A TestArguments object to pass to each TestType.
          shell_args: Any extra arguments to be passed to DumpRenderTree.
          options: A property dictionary as produced by optparse. The
              command-line options should match those expected by
              run_webkit_tests; they are typically passed via the
              run_webkit_tests.TestRunner class."""
        WatchableThread.__init__(self)
        self._port = port
        self._filename_list_queue = filename_list_queue
        self._result_queue = result_queue
        self._filename_list = []
        self._test_types = test_types
        self._test_args = test_args
        self._driver = None
        self._image_path = image_path
        self._shell_args = shell_args
        self._options = options
        self._directory_timing_stats = {}
        self._test_results = []
        self._num_tests = 0
        self._start_time = 0
        self._stop_time = 0

        # Current directory of tests we're running.
        self._current_dir = None
        # Number of tests in self._current_dir.
        self._num_tests_in_current_dir = None
        # Time at which we started running tests from self._current_dir.
        self._current_dir_start_time = None

    def get_directory_timing_stats(self):
        """Returns a dictionary mapping test directory to a tuple of
        (number of tests in that directory, time to run the tests)"""
        return self._directory_timing_stats

    def get_test_results(self):
        """Return the list of all tests run on this thread.

        This is used to calculate per-thread statistics.

        """
        return self._test_results

    def get_total_time(self):
        return max(self._stop_time - self._start_time, 0.0)

    def get_num_tests(self):
        return self._num_tests

    def run(self):
        """Delegate main work to a helper method and watch for uncaught
        exceptions."""
        self._covered_run()

    def _covered_run(self):
        # FIXME: this is a separate routine to work around a bug
        # in coverage: see http://bitbucket.org/ned/coveragepy/issue/85.
        self._thread_id = thread.get_ident()
        self._start_time = time.time()
        self._num_tests = 0
        try:
            _log.debug('%s starting' % (self.getName()))
            self._run(test_runner=None, result_summary=None)
            _log.debug('%s done (%d tests)' % (self.getName(),
                       self.get_num_tests()))
        except KeyboardInterrupt:
            self._exception_info = sys.exc_info()
            _log.debug("%s interrupted" % self.getName())
        except:
            # Save the exception for our caller to see.
            self._exception_info = sys.exc_info()
            self._stop_time = time.time()
            # Re-raise it and die.
            _log.error('%s dying, exception raised: %s' % (self.getName(),
                       self._exception_info))

        self._stop_time = time.time()

    def run_in_main_thread(self, test_runner, result_summary):
        """This hook allows us to run the tests from the main thread if
        --num-test-shells==1, instead of having to always run two or more
        threads. This allows us to debug the test harness without having to
        do multi-threaded debugging."""
        self._run(test_runner, result_summary)

    def _run(self, test_runner, result_summary):
        """Main work entry point of the thread. Basically we pull urls from the
        filename queue and run the tests until we run out of urls.

        If test_runner is not None, then we call test_runner.UpdateSummary()
        with the results of each test."""
        batch_size = self._options.batch_size
        batch_count = 0

        # Append tests we're running to the existing tests_run.txt file.
        # This is created in run_webkit_tests.py:_PrepareListsAndPrintOutput.
        tests_run_filename = os.path.join(self._options.results_directory,
                                          "tests_run.txt")
        tests_run_file = codecs.open(tests_run_filename, "a", "utf-8")

        while True:
            if self._canceled:
                _log.debug('Testing cancelled')
                tests_run_file.close()
                return

            if len(self._filename_list) is 0:
                if self._current_dir is not None:
                    self._directory_timing_stats[self._current_dir] = \
                        (self._num_tests_in_current_dir,
                         time.time() - self._current_dir_start_time)

                try:
                    self._current_dir, self._filename_list = \
                        self._filename_list_queue.get_nowait()
                except Queue.Empty:
                    self._kill_dump_render_tree()
                    tests_run_file.close()
                    return

                self._num_tests_in_current_dir = len(self._filename_list)
                self._current_dir_start_time = time.time()

            test_info = self._filename_list.pop()

            # We have a url, run tests.
            batch_count += 1
            self._num_tests += 1
            if self._options.run_singly:
                result = self._run_test_singly(test_info)
            else:
                result = self._run_test(test_info)

            filename = test_info.filename
            tests_run_file.write(filename + "\n")
            if result.failures:
                # Check and kill DumpRenderTree if we need to.
                if len([1 for f in result.failures
                        if f.should_kill_dump_render_tree()]):
                    self._kill_dump_render_tree()
                    # Reset the batch count since the shell just bounced.
                    batch_count = 0
                # Print the error message(s).
                error_str = '\n'.join(['  ' + f.message() for
                                       f in result.failures])
                _log.debug("%s %s failed:\n%s" % (self.getName(),
                           self._port.relative_test_filename(filename),
                           error_str))
            else:
                _log.debug("%s %s passed" % (self.getName(),
                           self._port.relative_test_filename(filename)))
            self._result_queue.put(result)

            if batch_size > 0 and batch_count > batch_size:
                # Bounce the shell and reset count.
                self._kill_dump_render_tree()
                batch_count = 0

            if test_runner:
                test_runner.update_summary(result_summary)

    def _run_test_singly(self, test_info):
        """Run a test in a separate thread, enforcing a hard time limit.

        Since we can only detect the termination of a thread, not any internal
        state or progress, we can only run per-test timeouts when running test
        files singly.

        Args:
          test_info: Object containing the test filename, uri and timeout

        Returns:
          A TestResult

        """
        worker = SingleTestThread(self._port, self._image_path,
                                  self._shell_args,
                                  test_info,
                                  self._test_types,
                                  self._test_args,
                                  self._options.configuration,
                                  self._options.results_directory)

        worker.start()

        thread_timeout = _milliseconds_to_seconds(
            _pad_timeout(test_info.timeout))
        thread._next_timeout = time.time() + thread_timeout
        worker.join(thread_timeout)
        if worker.isAlive():
            # If join() returned with the thread still running, the
            # DumpRenderTree is completely hung and there's nothing
            # more we can do with it.  We have to kill all the
            # DumpRenderTrees to free it up. If we're running more than
            # one DumpRenderTree thread, we'll end up killing the other
            # DumpRenderTrees too, introducing spurious crashes. We accept
            # that tradeoff in order to avoid losing the rest of this
            # thread's results.
            _log.error('Test thread hung: killing all DumpRenderTrees')
            worker._driver.stop()

        try:
            result = worker.get_test_result()
        except AttributeError, e:
            failures = []
            _log.error('Cannot get results of test: %s' %
                       test_info.filename)
            result = TestResult(test_info.filename, failures=[],
                                test_run_time=0, total_time_for_all_diffs=0,
                                time_for_diffs=0)

        return result

    def _run_test(self, test_info):
        """Run a single test file using a shared DumpRenderTree process.

        Args:
          test_info: Object containing the test filename, uri and timeout

        Returns:
          A list of TestFailure objects describing the error.

        """
        self._ensure_dump_render_tree_is_running()
        # The pixel_hash is used to avoid doing an image dump if the
        # checksums match, so it should be set to a blank value if we
        # are generating a new baseline.  (Otherwise, an image from a
        # previous run will be copied into the baseline.)
        image_hash = test_info.image_hash()
        if (image_hash and
            (self._test_args.new_baseline or self._test_args.reset_results or
            not self._options.pixel_tests)):
            image_hash = ""
        start = time.time()

        thread_timeout = _milliseconds_to_seconds(
             _pad_timeout(test_info.timeout))
        self._next_timeout = start + thread_timeout

        crash, timeout, actual_checksum, output, error = \
           self._driver.run_test(test_info.uri, test_info.timeout, image_hash)
        end = time.time()

        result = _process_output(self._port, test_info, self._test_types,
                                self._test_args, self._options.configuration,
                                self._options.results_directory, crash,
                                timeout, end - start, actual_checksum,
                                output, error)
        self._test_results.append(result)
        return result

    def _ensure_dump_render_tree_is_running(self):
        """Start the shared DumpRenderTree, if it's not running.

        This is not for use when running tests singly, since those each start
        a separate DumpRenderTree in their own thread.

        """
        # poll() is not threadsafe and can throw OSError due to:
        # http://bugs.python.org/issue1731717
        if (not self._driver or self._driver.poll() is not None):
            self._driver = self._port.create_driver(self._image_path, self._shell_args)
            self._driver.start()

    def _kill_dump_render_tree(self):
        """Kill the DumpRenderTree process if it's running."""
        if self._driver:
            self._driver.stop()
            self._driver = None
