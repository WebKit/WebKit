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
import os
import sys
import threading
import time

from webkitpy.common.host import Host
from webkitpy.layout_tests.controllers import manager_worker_broker
from webkitpy.layout_tests.controllers import single_test_runner
from webkitpy.layout_tests.models import test_expectations
from webkitpy.layout_tests.models import test_results
from webkitpy.layout_tests.views import metered_stream


_log = logging.getLogger(__name__)


class Worker(manager_worker_broker.AbstractWorker):
    def __init__(self, worker_connection, worker_number, results_directory, options):
        super(Worker, self).__init__(worker_connection, worker_number)
        self._results_directory = results_directory
        self._options = options
        self._port = None
        self._batch_size = None
        self._batch_count = None
        self._filesystem = None
        self._driver = None
        self._tests_run_file = None
        self._tests_run_filename = None
        self._log_messages = []
        self._logger = None
        self._log_handler = None

    def __del__(self):
        self.cleanup()

    def safe_init(self):
        """This method should only be called when it is is safe for the mixin
        to create state that can't be Pickled.

        This routine exists so that the mixin can be created and then marshaled
        across into a child process."""
        self._filesystem = self._port.host.filesystem
        self._batch_count = 0
        self._batch_size = self._options.batch_size or 0
        tests_run_filename = self._filesystem.join(self._results_directory, "tests_run%d.txt" % self._worker_number)
        self._tests_run_file = self._filesystem.open_text_file_for_writing(tests_run_filename)

    def _set_up_logging(self):
        self._logger = logging.getLogger()

        # The unix multiprocessing implementation clones the MeteredStream log handler
        # into the child process, so we need to remove it to avoid duplicate logging.
        for h in self._logger.handlers:
            # log handlers don't have names until python 2.7.
            if getattr(h, 'name', '') == metered_stream.LOG_HANDLER_NAME:
                self._logger.removeHandler(h)
                break

        self._logger.setLevel(logging.DEBUG if self._options.verbose else logging.INFO)
        self._log_handler = _WorkerLogHandler(self)
        self._logger.addHandler(self._log_handler)

    def run(self, host, set_up_logging):
        if not host:
            host = Host()

        # FIXME: this should move into manager_worker_broker.py.
        if set_up_logging:
            self._set_up_logging()

        options = self._options
        self._port = host.port_factory.get(options.platform, options)

        self.safe_init()
        try:
            _log.debug("%s starting" % self._name)
            super(Worker, self).run()
        finally:
            self.kill_driver()
            _log.debug("%s exiting" % self._name)
            self.cleanup()
            self._worker_connection.post_message('done', self._log_messages)

    def handle_test_list(self, src, list_name, test_list):
        start_time = time.time()
        num_tests = 0
        for test_input in test_list:
            self._update_test_input(test_input)
            self._run_test(test_input)
            num_tests += 1
            self._worker_connection.yield_to_broker()

        elapsed_time = time.time() - start_time
        self._worker_connection.post_message('finished_list', list_name, num_tests, elapsed_time)

    def handle_stop(self, src):
        self.stop_handling_messages()

    def _update_test_input(self, test_input):
        test_input.reference_files = self._port.reference_files(test_input.test_name)
        if test_input.reference_files:
            test_input.should_run_pixel_test = True
        elif self._options.pixel_tests:
            if self._options.skip_pixel_test_if_no_baseline:
                test_input.should_run_pixel_test = bool(self._port.expected_image(test_input.test_name))
            else:
                test_input.should_run_pixel_test = True
        else:
            test_input.should_run_pixel_test = False

    def _run_test(self, test_input):
        test_timeout_sec = self.timeout(test_input)
        start = time.time()
        self._worker_connection.post_message('started_test', test_input, test_timeout_sec)

        result = self.run_test_with_timeout(test_input, test_timeout_sec)

        elapsed_time = time.time() - start
        log_messages = self._log_messages
        self._log_messages = []
        self._worker_connection.post_message('finished_test', result, elapsed_time, log_messages)

        self.clean_up_after_test(test_input, result)

    def cleanup(self):
        _log.debug("%s cleaning up" % self._name)
        self.kill_driver()
        if self._tests_run_file:
            self._tests_run_file.close()
            self._tests_run_file = None
        if self._log_handler and self._logger:
            self._logger.removeHandler(self._log_handler)
        self._log_handler = None
        self._logger = None

    def timeout(self, test_input):
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

    def kill_driver(self):
        if self._driver:
            _log.debug("%s killing driver" % self._name)
            self._driver.stop()
            self._driver = None

    def run_test_with_timeout(self, test_input, timeout):
        if self._options.run_singly:
            return self._run_test_in_another_thread(test_input, timeout)
        return self._run_test_in_this_thread(test_input)

    def clean_up_after_test(self, test_input, result):
        self._batch_count += 1
        test_name = test_input.test_name
        self._tests_run_file.write(test_name + "\n")

        if result.failures:
            # Check and kill DumpRenderTree if we need to.
            if any([f.driver_needs_restart() for f in result.failures]):
                self.kill_driver()
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
            self.kill_driver()
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
                self.result = worker.run_single_test(driver, test_input)

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
            self.kill_driver()
        if not self._driver:
            self._driver = self._port.create_driver(self._worker_number)
        return self.run_single_test(self._driver, test_input)

    def run_single_test(self, driver, test_input):
        return single_test_runner.run_single_test(self._port, self._options,
            test_input, driver, self._name)


class _WorkerLogHandler(logging.Handler):
    def __init__(self, worker):
        logging.Handler.__init__(self)
        self._worker = worker
        self._pid = os.getpid()

    def emit(self, record):
        self._worker._log_messages.append(record)
