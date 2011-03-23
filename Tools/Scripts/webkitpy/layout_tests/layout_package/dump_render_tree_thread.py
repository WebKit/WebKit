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

"""This module implements a shared-memory, thread-based version of the worker
task in new-run-webkit-tests: it receives a list of tests from TestShellThread
and passes them one at a time to SingleTestRunner to execute."""

import logging
import Queue
import signal
import sys
import thread
import threading
import time

from webkitpy.layout_tests.layout_package import worker_mixin

_log = logging.getLogger("webkitpy.layout_tests.layout_package."
                         "dump_render_tree_thread")


class TestShellThread(threading.Thread, worker_mixin.WorkerMixin):
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
        threading.Thread.__init__(self)
        self._canceled = False
        self._exception_info = None
        self._next_timeout = None
        self._thread_id = None
        self._port = port
        self._options = options
        self._worker_number = worker_number
        self._name = worker_name
        self._filename_list_queue = filename_list_queue
        self._result_queue = result_queue
        self._current_group = None
        self._filename_list = []
        self._test_group_timing_stats = {}
        self._test_results = []
        self._num_tests = 0
        self._start_time = 0
        self._stop_time = 0
        self._http_lock_wait_begin = 0
        self._http_lock_wait_end = 0

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
        if self._next_timeout:
            return self._next_timeout + self._http_lock_wait_time()
        return self._next_timeout

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

        # Initialize the real state of the WorkerMixin now that we're executing
        # in the child thread. Technically, we could have called this during
        # __init__(), but we wait until now to match Worker.run().
        self.safe_init(self._port)

        while True:
            if self._canceled:
                _log.debug('Testing cancelled')
                self.cleanup()
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
                    self.cleanup()
                    return

                if self._current_group == "tests_to_http_lock":
                    self._http_lock_wait_begin = time.time()
                    self.start_servers_with_lock()
                    self._http_lock_wait_end = time.time()
                elif self._has_http_lock:
                    self.stop_servers_with_lock()

                self._num_tests_in_current_group = len(self._filename_list)
                self._current_group_start_time = time.time()

            test_input = self._filename_list.pop(0)

            # We have a url, run tests.
            self._num_tests += 1

            result = self.run_test_with_timeout(test_input, self.timeout(test_input))

            self.clean_up_after_test(test_input, result)
            self._test_results.append(result)
            self._result_queue.put(result.dumps())

            if test_runner:
                test_runner.update_summary(result_summary)
