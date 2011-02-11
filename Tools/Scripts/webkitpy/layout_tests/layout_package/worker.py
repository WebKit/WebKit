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
from webkitpy.layout_tests.layout_package import test_results


_log = logging.getLogger(__name__)


class Worker(manager_worker_broker.AbstractWorker):
    def __init__(self, worker_connection, worker_number, options):
        self._worker_connection = worker_connection
        self._worker_number = worker_number
        self._options = options
        self._name = 'worker/%d' % worker_number
        self._done = False
        self._port = None

    def _deferred_init(self, port):
        self._port = port

    def is_done(self):
        return self._done

    def name(self):
        return self._name

    def run(self, port):
        self._deferred_init(port)

        _log.debug("%s starting" % self._name)

        # FIXME: need to add in error handling, better logging.
        self._worker_connection.run_message_loop()
        self._worker_connection.post_message('done')

    def handle_test_list(self, src, list_name, test_list):
        # FIXME: check to see if we need to get the http lock.

        start_time = time.time()
        num_tests = 0
        for test_input in test_list:
            self._run_test(test_input)
            num_tests += 1
            self._worker_connection.yield_to_broker()

        elapsed_time = time.time() - start_time
        self._worker_connection.post_message('finished_list', list_name, num_tests, elapsed_time)

        # FIXME: release the lock if necessary

    def handle_stop(self, src):
        self._done = True

    def _run_test(self, test_input):

        # FIXME: get real timeout value from SingleTestRunner
        test_timeout_sec = int(test_input.timeout) / 1000
        start = time.time()
        self._worker_connection.post_message('started_test', test_input, test_timeout_sec)

        # FIXME: actually run the test.
        result = test_results.TestResult(test_input.filename, failures=[],
            test_run_time=0, total_time_for_all_diffs=0, time_for_diffs={})

        elapsed_time = time.time() - start

        # FIXME: update stats, check for failures.

        self._worker_connection.post_message('finished_test', result, elapsed_time)
