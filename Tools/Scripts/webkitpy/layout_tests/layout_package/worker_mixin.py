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

import logging
import sys
import threading
import time

from webkitpy.layout_tests.layout_package import single_test_runner
from webkitpy.layout_tests.layout_package import test_results

_log = logging.getLogger(__name__)


class WorkerMixin(object):
    """This class holds logic common to Worker and TestShellThread that
    doesn't directly have to do with running the tests (which is
    SingleTestRunner's responsibility. This class cannot stand on its own."""
    def __init__(self):
        assert False, "WorkerMixin can't be directly instantiated"

    def safe_init(self, port):
        """This method should only be called when it is is safe for the mixin
        to create state that can't be Pickled.

        This routine exists so that the mixin can be created and then marshaled
        across into a child process."""
        self._port = port
        self._filesystem = port._filesystem
        self._batch_count = 0
        self._batch_size = self._options.batch_size
        self._driver = None
        tests_run_filename = self._filesystem.join(port.results_directory(),
                                                   "tests_run%d.txt" % self._worker_number)
        self._tests_run_file = self._filesystem.open_text_file_for_writing(tests_run_filename)

        # FIXME: it's goofy that we have to track this at all, but it's due to
        # the awkward logic in TestShellThread._run(). When we remove that
        # file, we should rewrite this code so that caller keeps track of whether
        # the lock is held.
        self._has_http_lock = False

    def cleanup(self):
        if self._driver:
            self.kill_driver()
        if self._has_http_lock:
            self.stop_servers_with_lock()
        if self._tests_run_file:
            self._tests_run_file.close()
            self._tests_run_file = None

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

    def start_servers_with_lock(self):
        _log.debug('Acquiring http lock ...')
        self._port.acquire_http_lock()
        _log.debug('Starting HTTP server ...')
        self._port.start_http_server()
        _log.debug('Starting WebSocket server ...')
        self._port.start_websocket_server()
        self._has_http_lock = True

    def stop_servers_with_lock(self):
        if self._has_http_lock:
            _log.debug('Stopping HTTP server ...')
            self._port.stop_http_server()
            _log.debug('Stopping WebSocket server ...')
            self._port.stop_websocket_server()
            _log.debug('Releasing server lock ...')
            self._port.release_http_lock()
            self._has_http_lock = False

    def kill_driver(self):
        if self._driver:
            self._driver.stop()
            self._driver = None

    def run_test_with_timeout(self, test_input, timeout):
        if self._options.run_singly:
            return self._run_test_in_another_thread(test_input, timeout)
        else:
            return self._run_test_in_this_thread(test_input)
        return result

    def clean_up_after_test(self, test_input, result):
        self._batch_count += 1
        self._tests_run_file.write(test_input.filename + "\n")
        test_name = self._port.relative_test_filename(test_input.filename)

        if result.failures:
            # Check and kill DumpRenderTree if we need to.
            if any([f.should_kill_dump_render_tree() for f in result.failures]):
                self.kill_driver()
                # Reset the batch count since the shell just bounced.
                self._batch_count = 0

            # Print the error message(s).
            _log.debug("%s %s failed:" % (self._name, test_name))
            for f in result.failures:
                _log.debug("%s  %s" % (self._name, f.message()))
        else:
            _log.debug("%s %s passed" % (self._name, test_name))

        if self._batch_size > 0 and self._batch_count >= self._batch_size:
            # Bounce the shell and reset count.
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

        driver = worker._port.create_driver(worker._worker_number)
        driver.start()

        class SingleTestThread(threading.Thread):
            def run(self):
                self.result = worker._run_single_test(driver, test_input)

        thread = SingleTestThread()
        thread.start()
        thread.join(thread_timeout_sec)
        result = getattr(thread, 'result', None)
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
            result = test_results.TestResult(test_input.filename, failures=[], test_run_time=0)
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
        return self._run_single_test(self._driver, test_input)

    def _run_single_test(self, driver, test_input):
        return single_test_runner.run_single_test(self._port, self._options,
            test_input, driver, self._name)
