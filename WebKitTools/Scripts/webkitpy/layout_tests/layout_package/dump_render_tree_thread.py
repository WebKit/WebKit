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

        self._batch_count = 0
        self._batch_size = self._options.batch_size
        self._driver = None
        self._have_http_lock = False

        self._test_runner = None
        self._result_summary = None
        self._test_list_timing_stats = {}
        self._test_results = []
        self._num_tests = 0
        self._start_time = 0
        self._stop_time = 0
        self._http_lock_wait_begin = 0
        self._http_lock_wait_end = 0

        self._test_types = []
        for cls in self._get_test_type_classes():
            self._test_types.append(cls(self._port,
                                        self._options.results_directory))
        self._test_args = self._get_test_args(worker_number)

        # Append tests we're running to the existing tests_run.txt file.
        # This is created in run_webkit_tests.py:_PrepareListsAndPrintOutput.
        tests_run_filename = os.path.join(self._options.results_directory,
                                          "tests_run.txt")
        self._tests_run_file = codecs.open(tests_run_filename, "a", "utf-8")

    def __del__(self):
        self._cleanup()

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
        return self._test_list_timing_stats

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

    def next_timeout(self):
        """Return the time the test is supposed to finish by."""
        if self._next_timeout:
            return self._next_timeout + self._http_lock_wait_time()
        return self._next_timeout

    def run(self):
        """Delegate main work to a helper method and watch for uncaught
        exceptions."""
        self._covered_run()

    def _covered_run(self):
        # FIXME: this is a separate routine to work around a bug
        # in coverage: see http://bitbucket.org/ned/coveragepy/issue/85.
        self._thread_id = thread.get_ident()
        self._start_time = time.time()
        try:
            _log.debug('%s starting' % (self._name))
            self._run(test_runner=None, result_summary=None)
            _log.debug('%s done (%d tests)' % (self._name, self._num_tests))
        except KeyboardInterrupt:
            self._exception_info = sys.exc_info()
            _log.debug("%s interrupted" % self._name)
        except:
            # Save the exception for our caller to see.
            self._exception_info = sys.exc_info()
            self._stop_time = time.time()
            _log.error('%s dying, exception raised' % self._name)

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
        with the results of each test during _tear_down_test(), below."""
        self._test_runner = test_runner
        self._result_summary = result_summary

        while not self._canceled:
            try:
                current_group, filename_list = \
                    self._filename_list_queue.get_nowait()
                self.handle_test_list(current_group, filename_list)
            except Queue.Empty:
                break

        if self._canceled:
            _log.debug('Testing canceled')

        self._cleanup()

    def _cleanup(self):
        self._kill_dump_render_tree()
        if self._have_http_lock:
            self._stop_servers_with_lock()
        if self._tests_run_file:
            self._tests_run_file.close()
            self._tests_run_file = None

    def handle_test_list(self, list_name, test_list):
        if list_name == "tests_to_http_lock":
            self._start_servers_with_lock()

        start_time = time.time()
        num_tests = 0
        for test_input in test_list:
            self._run_test(test_input)
            if self._canceled:
                break
            num_tests += 1

        elapsed_time = time.time() - start_time

        if self._have_http_lock:
            self._stop_servers_with_lock()

        self._test_list_timing_stats[list_name] = \
           (num_tests, elapsed_time)

    def _run_test(self, test_input):
        self._set_up_test(test_input)

        # We calculate how long we expect the test to take.
        #
        # The DumpRenderTree watchdog uses 2.5x the timeout; we want to be
        # larger than that. We also add a little more padding if we're
        # running tests in a separate thread.
        #
        # Note that we need to convert the test timeout from a
        # string value in milliseconds to a float for Python.
        driver_timeout_sec = 3.0 * float(test_input.timeout) / 1000.0
        thread_padding_sec = 1.0
        thread_timeout_sec = driver_timeout_sec + thread_padding_sec
        if self._options.run_singly:
            test_timeout_sec = thread_timeout_sec
        else:
            test_timeout_sec = driver_timeout_sec

        start = time.time()
        self._next_timeout = start + test_timeout_sec

        if self._options.run_singly:
            result = self._run_test_in_another_thread(test_input,
                                                      thread_timeout_sec)
        else:
            result = self._run_test_in_this_thread(test_input)

        self._tear_down_test(test_input, result)

    def _set_up_test(self, test_input):
        test_input.uri = self._port.filename_to_uri(test_input.filename)
        if self._should_fetch_expected_checksum():
            test_input.image_checksum = self._port.expected_checksum(
                test_input.filename)

    def _should_fetch_expected_checksum(self):
        return (self._options.pixel_tests and not
                (self._options.new_baseline or self._options.reset_results))

    def _run_test_in_another_thread(self, test_input, thread_timeout_sec):
        """Run a test in a separate thread, enforcing a hard time limit.

        Since we can only detect the termination of a thread, not any internal
        state or progress, we can only run per-test timeouts when running test
        files singly.

        Args:
          test_input: Object containing the test filename and timeout
          thread_timeout_sec: time to wait before killing the driver process.
        Returns:
          A TestResult
        """
        worker = self
        result = None

        driver = worker._port.create_driver(worker._worker_number)
        driver.start()

        class SingleTestThread(threading.Thread):
            def run(self):
                result = worker._run_single_test(driver, test_input)

        thread = SingleTestThread()
        thread.start()
        thread.join(thread_timeout_sec)
        if thread.isAlive():
            # If join() returned with the thread still running, the
            # DumpRenderTree is completely hung and there's nothing
            # more we can do with it.  We have to kill all the
            # DumpRenderTrees to free it up. If we're running more than
            # one DumpRenderTree thread, we'll end up killing the other
            # DumpRenderTrees too, introducing spurious crashes. We accept
            # that tradeoff in order to avoid losing the rest of this
            # thread's results.
            _log.error('Test thread hung: killing all DumpRenderTrees')

        driver.stop()

        if not result:
            result = test_results.TestResult(test_input.filename, failures=[],
                test_run_time=0, total_time_for_all_diffs=0, time_for_diffs={})

        return result

    def _run_test_in_this_thread(self, test_input):
        """Run a single test file using a shared DumpRenderTree process.

        Args:
          test_input: Object containing the test filename, uri and timeout

        Returns: a TestResult object.
        """
        # poll() is not threadsafe and can throw OSError due to:
        # http://bugs.python.org/issue1731717
        if not self._driver or self._driver.poll() is not None:
            self._driver = self._port.create_driver(self._worker_number)
            self._driver.start()

        test_result = self._run_single_test(test_input, self._driver)
        self._test_results.append(test_result)
        return test_result

    def _run_single_test(self, test_input, driver):
        # The image hash is used to avoid doing an image dump if the
        # checksums match, so it should be set to a blank value if we
        # are generating a new baseline.  (Otherwise, an image from a
        # previous run will be copied into the baseline."""
        if self._should_fetch_expected_checksum():
            test_input.image_hash = self._port.expected_checksum(
                test_input.filename)
        test_output = driver.run_test(test_input)
        return self._process_output(test_input.filename, test_output)

    def _process_output(self, test_filename, test_output):
        """Receives the output from a DumpRenderTree process, subjects it to a
        number of tests, and returns a list of failure types the test produced.

        Args:
        test_filename: full path to the test in question.
        test_output: a TestOutput object containing the output of the test

        Returns: a TestResult object
        """
        failures = []

        if test_output.crash:
            failures.append(test_failures.FailureCrash())
        if test_output.timeout:
            failures.append(test_failures.FailureTimeout())

        test_name = self._port.relative_test_filename(test_filename)
        if test_output.crash:
            _log.debug("%s Stacktrace for %s:\n%s" %
                       (self._name, test_name, test_output.error))
            filename = os.path.join(self._options.results_directory, test_name)
            filename = os.path.splitext(filename)[0] + "-stack.txt"
            self._port.maybe_make_directory(os.path.split(filename)[0])
            with codecs.open(filename, "wb", "utf-8") as file:
                file.write(test_output.error)
        elif test_output.error:
            _log.debug("%s %s output stderr lines:\n%s" %
                       (self._name, test_name, test_output.error))

        expected_test_output = self._expected_test_output(test_filename)

        # Check the output and save the results.
        start_time = time.time()
        time_for_diffs = {}
        for test_type in self._test_types:
            start_diff_time = time.time()
            new_failures = test_type.compare_output(self._port,
                                                    test_filename,
                                                    self._test_args,
                                                    test_output,
                                                    expected_test_output)
            # Don't add any more failures if we already have a crash, so we
            # don't double-report those tests. We do double-report for timeouts
            # since we still want to see the text and image output.
            if not test_output.crash:
                failures.extend(new_failures)
            time_for_diffs[test_type.__class__.__name__] = (
                time.time() - start_diff_time)

        total_time_for_all_diffs = time.time() - start_diff_time
        return test_results.TestResult(test_filename,
                                       failures,
                                       test_output.test_time,
                                       total_time_for_all_diffs,
                                       time_for_diffs)

    def _expected_test_output(self, filename):
        """Returns an expected TestOutput object."""
        return test_output.TestOutput(self._port.expected_text(filename),
                                    self._port.expected_image(filename),
                                    self._port.expected_checksum(filename))

    def _tear_down_test(self, test_input, result):
        self._num_tests += 1
        self._batch_count += 1
        self._tests_run_file.write(test_input.filename + "\n")
        test_name = self._port.relative_test_filename(test_input.filename)

        if result.failures:
            # Check and kill DumpRenderTree if we need to.
            if any([f.should_kill_dump_render_tree() for f in result.failures]):
                self._kill_dump_render_tree()
                # Reset the batch count since the shell just bounced.
                self._batch_count = 0

            # Print the error message(s).
            _log.debug("%s %s failed:" % (self._name, test_name))
            for f in result.failures:
                _log.debug("%s  %s" % (self._name, f.message()))
        else:
            _log.debug("%s %s passed" % (self._name, test_name))

        self._result_queue.put(result.dumps())

        if self._batch_size > 0 and self._batch_count >= self._batch_size:
            # Bounce the shell and reset count.
            self._kill_dump_render_tree()
            self._batch_count = 0

        if self._test_runner:
            self._test_runner.update_summary(self._result_summary)

    def _start_servers_with_lock(self):
        self._http_lock_wait_begin = time.time()
        _log.debug('Acquiring http lock ...')
        self._port.acquire_http_lock()
        _log.debug('Starting HTTP server ...')
        self._port.start_http_server()
        _log.debug('Starting WebSocket server ...')
        self._port.start_websocket_server()
        self._http_lock_wait_end = time.time()
        self._have_http_lock = True

    def _http_lock_wait_time(self):
        """Return the time what http locking takes."""
        if self._http_lock_wait_begin == 0:
            return 0
        if self._http_lock_wait_end == 0:
            return time.time() - self._http_lock_wait_begin
        return self._http_lock_wait_end - self._http_lock_wait_begin

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
