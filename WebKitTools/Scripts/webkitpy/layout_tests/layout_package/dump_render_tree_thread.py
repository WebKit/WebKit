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


from webkitpy.layout_tests.test_types import image_diff
from webkitpy.layout_tests.test_types import test_type_base
from webkitpy.layout_tests.test_types import text_diff

import test_failures
import test_output
import test_results

_log = logging.getLogger("webkitpy.layout_tests.layout_package."
                         "dump_render_tree_thread")


def _expected_test_output(port, filename):
    """Returns an expected TestOutput object."""
    return test_output.TestOutput(port.expected_text(filename),
                                  port.expected_image(filename),
                                  port.expected_checksum(filename))

def _process_output(port, options, test_input, test_types, test_args,
                    test_output, worker_name):
    """Receives the output from a DumpRenderTree process, subjects it to a
    number of tests, and returns a list of failure types the test produced.

    Args:
      port: port-specific hooks
      options: command line options argument from optparse
      proc: an active DumpRenderTree process
      test_input: Object containing the test filename and timeout
      test_types: list of test types to subject the output to
      test_args: arguments to be passed to each test
      test_output: a TestOutput object containing the output of the test
      worker_name: worker name for logging

    Returns: a TestResult object
    """
    failures = []

    if test_output.crash:
        failures.append(test_failures.FailureCrash())
    if test_output.timeout:
        failures.append(test_failures.FailureTimeout())

    test_name = port.relative_test_filename(test_input.filename)
    if test_output.crash:
        _log.debug("%s Stacktrace for %s:\n%s" % (worker_name, test_name,
                                                  test_output.error))
        filename = os.path.join(options.results_directory, test_name)
        filename = os.path.splitext(filename)[0] + "-stack.txt"
        port.maybe_make_directory(os.path.split(filename)[0])
        with codecs.open(filename, "wb", "utf-8") as file:
            file.write(test_output.error)
    elif test_output.error:
        _log.debug("%s %s output stderr lines:\n%s" % (worker_name, test_name,
                                                       test_output.error))

    expected_test_output = _expected_test_output(port, test_input.filename)

    # Check the output and save the results.
    start_time = time.time()
    time_for_diffs = {}
    for test_type in test_types:
        start_diff_time = time.time()
        new_failures = test_type.compare_output(port, test_input.filename,
                                                test_args, test_output,
                                                expected_test_output)
        # Don't add any more failures if we already have a crash, so we don't
        # double-report those tests. We do double-report for timeouts since
        # we still want to see the text and image output.
        if not test_output.crash:
            failures.extend(new_failures)
        time_for_diffs[test_type.__class__.__name__] = (
            time.time() - start_diff_time)

    total_time_for_all_diffs = time.time() - start_diff_time
    return test_results.TestResult(test_input.filename, failures, test_output.test_time,
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


def _should_fetch_expected_checksum(options):
    return options.pixel_tests and not (options.new_baseline or options.reset_results)


def _run_single_test(port, options, test_input, test_types, test_args, driver, worker_name):
    # FIXME: Pull this into TestShellThread._run().

    # The image hash is used to avoid doing an image dump if the
    # checksums match, so it should be set to a blank value if we
    # are generating a new baseline.  (Otherwise, an image from a
    # previous run will be copied into the baseline."""
    if _should_fetch_expected_checksum(options):
        test_input.image_hash = port.expected_checksum(test_input.filename)
    test_output = driver.run_test(test_input)
    return _process_output(port, options, test_input, test_types, test_args,
                           test_output, worker_name)


class SingleTestThread(threading.Thread):
    """Thread wrapper for running a single test file."""

    def __init__(self, port, options, worker_number, worker_name,
                 test_input, test_types, test_args):
        """
        Args:
          port: object implementing port-specific hooks
          options: command line argument object from optparse
          worker_number: worker number for tests
          worker_name: for logging
          test_input: Object containing the test filename and timeout
          test_types: A list of TestType objects to run the test output
              against.
          test_args: A TestArguments object to pass to each TestType.
        """

        threading.Thread.__init__(self)
        self._port = port
        self._options = options
        self._test_input = test_input
        self._test_types = test_types
        self._test_args = test_args
        self._driver = None
        self._worker_number = worker_number
        self._name = worker_name

    def run(self):
        self._covered_run()

    def _covered_run(self):
        # FIXME: this is a separate routine to work around a bug
        # in coverage: see http://bitbucket.org/ned/coveragepy/issue/85.
        self._driver = self._port.create_driver(self._worker_number)
        self._driver.start()
        self._test_result = _run_single_test(self._port, self._options,
                                             self._test_input, self._test_types,
                                             self._test_args, self._driver,
                                             self._name)
        self._driver.stop()

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
    def __init__(self, port, options, worker_number, worker_name,
                 filename_list_queue, result_queue):
        """Initialize all the local state for this DumpRenderTree thread.

        Args:
          port: interface to port-specific hooks
          options: command line options argument from optparse
          worker_number: identifier for a particular worker thread.
          worker_name: for logging.
          filename_list_queue: A thread safe Queue class that contains lists
              of tuples of (filename, uri) pairs.
          result_queue: A thread safe Queue class that will contain
              serialized TestResult objects.
        """
        WatchableThread.__init__(self)
        self._port = port
        self._options = options
        self._worker_number = worker_number
        self._name = worker_name
        self._filename_list_queue = filename_list_queue
        self._result_queue = result_queue
        self._filename_list = []
        self._driver = None
        self._test_group_timing_stats = {}
        self._test_results = []
        self._num_tests = 0
        self._start_time = 0
        self._stop_time = 0
        self._have_http_lock = False
        self._http_lock_wait_begin = 0
        self._http_lock_wait_end = 0

        self._test_types = []
        for cls in self._get_test_type_classes():
            self._test_types.append(cls(self._port,
                                        self._options.results_directory))
        self._test_args = self._get_test_args(worker_number)

        # Current group of tests we're running.
        self._current_group = None
        # Number of tests in self._current_group.
        self._num_tests_in_current_group = None
        # Time at which we started running tests from self._current_group.
        self._current_group_start_time = None

    def _get_test_args(self, worker_number):
        """Returns the tuple of arguments for tests and for DumpRenderTree."""
        test_args = test_type_base.TestArguments()
        test_args.new_baseline = self._options.new_baseline
        test_args.reset_results = self._options.reset_results

        return test_args

    def _get_test_type_classes(self):
        classes = [text_diff.TestTextDiff]
        if self._options.pixel_tests:
            classes.append(image_diff.ImageDiff)
        return classes

    def get_test_group_timing_stats(self):
        """Returns a dictionary mapping test group to a tuple of
        (number of tests in that group, time to run the tests)"""
        return self._test_group_timing_stats

    def get_test_results(self):
        """Return the list of all tests run on this thread.

        This is used to calculate per-thread statistics.

        """
        return self._test_results

    def get_total_time(self):
        return max(self._stop_time - self._start_time -
                   self._http_lock_wait_time(), 0.0)

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
            _log.error('%s dying, exception raised' % self.getName())

        self._stop_time = time.time()

    def run_in_main_thread(self, test_runner, result_summary):
        """This hook allows us to run the tests from the main thread if
        --num-test-shells==1, instead of having to always run two or more
        threads. This allows us to debug the test harness without having to
        do multi-threaded debugging."""
        self._run(test_runner, result_summary)

    def cancel(self):
        """Clean up http lock and set a flag telling this thread to quit."""
        self._stop_servers_with_lock()
        WatchableThread.cancel(self)

    def next_timeout(self):
        """Return the time the test is supposed to finish by."""
        if self._next_timeout:
            return self._next_timeout + self._http_lock_wait_time()
        return self._next_timeout

    def _http_lock_wait_time(self):
        """Return the time what http locking takes."""
        if self._http_lock_wait_begin == 0:
            return 0
        if self._http_lock_wait_end == 0:
            return time.time() - self._http_lock_wait_begin
        return self._http_lock_wait_end - self._http_lock_wait_begin

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
                if self._current_group is not None:
                    self._test_group_timing_stats[self._current_group] = \
                        (self._num_tests_in_current_group,
                         time.time() - self._current_group_start_time)

                try:
                    self._current_group, self._filename_list = \
                        self._filename_list_queue.get_nowait()
                except Queue.Empty:
                    self._stop_servers_with_lock()
                    self._kill_dump_render_tree()
                    tests_run_file.close()
                    return

                if self._current_group == "tests_to_http_lock":
                    self._start_servers_with_lock()
                elif self._have_http_lock:
                    self._stop_servers_with_lock()

                self._num_tests_in_current_group = len(self._filename_list)
                self._current_group_start_time = time.time()

            test_input = self._filename_list.pop()

            # We have a url, run tests.
            batch_count += 1
            self._num_tests += 1
            if self._options.run_singly:
                result = self._run_test_in_another_thread(test_input)
            else:
                result = self._run_test_in_this_thread(test_input)

            filename = test_input.filename
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
            self._result_queue.put(result.dumps())

            if batch_size > 0 and batch_count >= batch_size:
                # Bounce the shell and reset count.
                self._kill_dump_render_tree()
                batch_count = 0

            if test_runner:
                test_runner.update_summary(result_summary)

    def _run_test_in_another_thread(self, test_input):
        """Run a test in a separate thread, enforcing a hard time limit.

        Since we can only detect the termination of a thread, not any internal
        state or progress, we can only run per-test timeouts when running test
        files singly.

        Args:
          test_input: Object containing the test filename and timeout

        Returns:
          A TestResult
        """
        worker = SingleTestThread(self._port,
                                  self._options,
                                  self._worker_number,
                                  self._name,
                                  test_input,
                                  self._test_types,
                                  self._test_args)

        worker.start()

        thread_timeout = _milliseconds_to_seconds(
            _pad_timeout(int(test_input.timeout)))
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
            if worker._driver:
                worker._driver.stop()

        try:
            result = worker.get_test_result()
        except AttributeError, e:
            # This gets raised if the worker thread has already exited.
            failures = []
            _log.error('Cannot get results of test: %s' %
                       test_input.filename)
            result = test_results.TestResult(test_input.filename, failures=[],
                test_run_time=0, total_time_for_all_diffs=0, time_for_diffs={})

        return result

    def _run_test_in_this_thread(self, test_input):
        """Run a single test file using a shared DumpRenderTree process.

        Args:
          test_input: Object containing the test filename, uri and timeout

        Returns: a TestResult object.
        """
        self._ensure_dump_render_tree_is_running()
        thread_timeout = _milliseconds_to_seconds(
             _pad_timeout(int(test_input.timeout)))
        self._next_timeout = time.time() + thread_timeout
        test_result = _run_single_test(self._port, self._options, test_input,
                                       self._test_types, self._test_args,
                                       self._driver, self._name)
        self._test_results.append(test_result)
        return test_result

    def _ensure_dump_render_tree_is_running(self):
        """Start the shared DumpRenderTree, if it's not running.

        This is not for use when running tests singly, since those each start
        a separate DumpRenderTree in their own thread.

        """
        # poll() is not threadsafe and can throw OSError due to:
        # http://bugs.python.org/issue1731717
        if not self._driver or self._driver.poll() is not None:
            self._driver = self._port.create_driver(self._worker_number)
            self._driver.start()

    def _start_servers_with_lock(self):
        """Acquire http lock and start the servers."""
        self._http_lock_wait_begin = time.time()
        _log.debug('Acquire http lock ...')
        self._port.acquire_http_lock()
        _log.debug('Starting HTTP server ...')
        self._port.start_http_server()
        _log.debug('Starting WebSocket server ...')
        self._port.start_websocket_server()
        self._http_lock_wait_end = time.time()
        self._have_http_lock = True

    def _stop_servers_with_lock(self):
        """Stop the servers and release http lock."""
        if self._have_http_lock:
            _log.debug('Stopping HTTP server ...')
            self._port.stop_http_server()
            _log.debug('Stopping WebSocket server ...')
            self._port.stop_websocket_server()
            _log.debug('Release http lock ...')
            self._port.release_http_lock()
            self._have_http_lock = False

    def _kill_dump_render_tree(self):
        """Kill the DumpRenderTree process if it's running."""
        if self._driver:
            self._driver.stop()
            self._driver = None
