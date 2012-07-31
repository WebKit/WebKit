# Copyright (C) 2011 Google Inc. All rights reserved.
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

"""Handle messages from the Manager and executes actual tests."""

import logging
import threading
import time

from webkitpy.layout_tests.controllers import single_test_runner
from webkitpy.layout_tests.models import test_expectations
from webkitpy.layout_tests.models import test_results


_log = logging.getLogger(__name__)


class Worker(object):
    def __init__(self, caller, results_directory, options):
        self._caller = caller
        self._worker_number = caller.worker_number
        self._name = caller.name
        self._results_directory = results_directory
        self._options = options

        # The remaining fields are initialized in start()
        self._host = None
        self._port = None
        self._batch_size = None
        self._batch_count = None
        self._filesystem = None
        self._driver = None
        self._tests_run_file = None
        self._tests_run_filename = None

    def __del__(self):
        self.stop()

    def start(self):
        """This method is called when the object is starting to be used and it is safe
        for the object to create state that does not need to be pickled (usually this means
        it is called in a child process)."""
        self._host = self._caller.host
        self._filesystem = self._host.filesystem
        self._port = self._host.port_factory.get(self._options.platform, self._options)

        self._batch_count = 0
        self._batch_size = self._options.batch_size or 0
        tests_run_filename = self._filesystem.join(self._results_directory, "tests_run%d.txt" % self._worker_number)
        self._tests_run_file = self._filesystem.open_text_file_for_writing(tests_run_filename)

    def handle(self, name, source, test_list_name, test_inputs):
        assert name == 'test_list'
        start_time = time.time()
        for test_input in test_inputs:
            self._run_test(test_input)
        elapsed_time = time.time() - start_time
        self._caller.post('finished_test_list', test_list_name, len(test_inputs), elapsed_time)

    def _update_test_input(self, test_input):
        if test_input.reference_files is None:
            # Lazy initialization.
            test_input.reference_files = self._port.reference_files(test_input.test_name)
        if test_input.reference_files:
            test_input.should_run_pixel_test = True
        else:
            test_input.should_run_pixel_test = self._port.should_run_as_pixel_test(test_input)

    def _run_test(self, test_input):
        self._update_test_input(test_input)
        test_timeout_sec = self._timeout(test_input)
        start = time.time()
        self._caller.post('started_test', test_input, test_timeout_sec)

        result = self._run_test_with_timeout(test_input, test_timeout_sec)

        elapsed_time = time.time() - start
        self._caller.post('finished_test', result, elapsed_time)

        self._clean_up_after_test(test_input, result)

    def stop(self):
        _log.debug("%s cleaning up" % self._name)
        self._kill_driver()
        if self._tests_run_file:
            self._tests_run_file.close()
            self._tests_run_file = None

    def _timeout(self, test_input):
        """Compute the appropriate timeout value for a test."""
        # The DumpRenderTree watchdog uses 2.5x the timeout; we want to be
        # larger than that. We also add a little more padding if we're
        # running tests in a separate thread.
        #
        # Note that we need to convert the test timeout from a
        # string value in milliseconds to a float for Python.
        driver_timeout_sec = 3.0 * float(test_input.timeout) / 1000.0
        if not self._options.run_singly:
            return driver_timeout_sec

        thread_padding_sec = 1.0
        thread_timeout_sec = driver_timeout_sec + thread_padding_sec
        return thread_timeout_sec

    def _kill_driver(self):
        # Be careful about how and when we kill the driver; if driver.stop()
        # raises an exception, this routine may get re-entered via __del__.
        driver = self._driver
        self._driver = None
        if driver:
            _log.debug("%s killing driver" % self._name)
            driver.stop()

    def _run_test_with_timeout(self, test_input, timeout):
        if self._options.run_singly:
            return self._run_test_in_another_thread(test_input, timeout)
        return self._run_test_in_this_thread(test_input)

    def _clean_up_after_test(self, test_input, result):
        self._batch_count += 1
        test_name = test_input.test_name
        self._tests_run_file.write(test_name + "\n")

        if result.failures:
            # Check and kill DumpRenderTree if we need to.
            if any([f.driver_needs_restart() for f in result.failures]):
                self._kill_driver()
                # Reset the batch count since the shell just bounced.
                self._batch_count = 0

            # Print the error message(s).
            _log.debug("%s %s failed:" % (self._name, test_name))
            for f in result.failures:
                _log.debug("%s  %s" % (self._name, f.message()))
        elif result.type == test_expectations.SKIP:
            _log.debug("%s %s skipped" % (self._name, test_name))
        else:
            _log.debug("%s %s passed" % (self._name, test_name))

        if self._batch_size > 0 and self._batch_count >= self._batch_size:
            self._kill_driver()
            self._batch_count = 0

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

        driver = self._port.create_driver(self._worker_number)

        class SingleTestThread(threading.Thread):
            def __init__(self):
                threading.Thread.__init__(self)
                self.result = None

            def run(self):
                self.result = worker._run_single_test(driver, test_input)

        thread = SingleTestThread()
        thread.start()
        thread.join(thread_timeout_sec)
        result = thread.result
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
            result = test_results.TestResult(test_input.test_name, failures=[], test_run_time=0)
        return result

    def _run_test_in_this_thread(self, test_input):
        """Run a single test file using a shared DumpRenderTree process.

        Args:
          test_input: Object containing the test filename, uri and timeout

        Returns: a TestResult object.
        """
        if self._driver and self._driver.has_crashed():
            self._kill_driver()
        if not self._driver:
            self._driver = self._port.create_driver(self._worker_number)
        return self._run_single_test(self._driver, test_input)

    def _run_single_test(self, driver, test_input):
        return single_test_runner.run_single_test(self._port, self._options,
            test_input, driver, self._name)
