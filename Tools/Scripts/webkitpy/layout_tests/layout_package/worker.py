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

"""Handle messages from the TestRunner and execute actual tests."""

import logging
import sys
import time

from webkitpy.common.system import stack_utils

from webkitpy.layout_tests.layout_package import manager_worker_broker
from webkitpy.layout_tests.layout_package import worker_mixin


_log = logging.getLogger(__name__)


class Worker(manager_worker_broker.AbstractWorker, worker_mixin.WorkerMixin):
    def __init__(self, worker_connection, worker_number, options):
        self._worker_connection = worker_connection
        self._worker_number = worker_number
        self._options = options
        self._name = 'worker/%d' % worker_number
        self._done = False
        self._canceled = False
        self._port = None

    def __del__(self):
        self.cleanup()

    def cancel(self):
        """Attempt to abort processing (best effort)."""
        self._canceled = True

    def is_done(self):
        return self._done or self._canceled

    def name(self):
        return self._name

    def run(self, port):
        self.safe_init(port)

        exception_msg = ""
        _log.debug("%s starting" % self._name)

        try:
            self._worker_connection.run_message_loop()
            if not self.is_done():
                raise AssertionError("%s: ran out of messages in worker queue."
                                     % self._name)
        except KeyboardInterrupt:
            exception_msg = ", interrupted"
        except:
            exception_msg = ", exception raised"
        finally:
            _log.debug("%s done%s" % (self._name, exception_msg))
            if exception_msg:
                exception_type, exception_value, exception_traceback = sys.exc_info()
                stack_utils.log_traceback(_log.error, exception_traceback)
                # FIXME: Figure out how to send a message with a traceback.
                self._worker_connection.post_message('exception',
                    (exception_type, exception_value, None))
            self._worker_connection.post_message('done')

    def handle_test_list(self, src, list_name, test_list):
        if list_name == "tests_to_http_lock":
            self.start_servers_with_lock()

        start_time = time.time()
        num_tests = 0
        for test_input in test_list:
            self._run_test(test_input)
            num_tests += 1
            self._worker_connection.yield_to_broker()

        elapsed_time = time.time() - start_time
        self._worker_connection.post_message('finished_list', list_name, num_tests, elapsed_time)

        if self._has_http_lock:
            self.stop_servers_with_lock()

    def handle_stop(self, src):
        self._done = True

    def _run_test(self, test_input):
        test_timeout_sec = self.timeout(test_input)
        start = time.time()
        self._worker_connection.post_message('started_test', test_input, test_timeout_sec)

        result = self.run_test_with_timeout(test_input, test_timeout_sec)

        elapsed_time = time.time() - start
        self._worker_connection.post_message('finished_test', result, elapsed_time)

        self.clean_up_after_test(test_input, result)
