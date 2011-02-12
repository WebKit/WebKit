#!/usr/bin/env python
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

"""
The TestRunner2 package is an alternate implementation of the TestRunner
class that uses the manager_worker_broker module to send sets of tests to
workers and receive their completion messages accordingly.
"""

import logging


from webkitpy.layout_tests.layout_package import manager_worker_broker
from webkitpy.layout_tests.layout_package import test_runner
from webkitpy.layout_tests.layout_package import worker

_log = logging.getLogger(__name__)


class TestRunner2(test_runner.TestRunner):
    def __init__(self, port, options, printer):
        test_runner.TestRunner.__init__(self, port, options, printer)
        self._all_results = []
        self._group_stats = {}
        self._current_result_summary = None
        self._done = False

    def is_done(self):
        return self._done

    def name(self):
        return 'TestRunner2'

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
        self._current_result_summary = result_summary

        # FIXME: shard properly.

        # FIXME: should shard_tests return a list of objects rather than tuples?
        test_lists = self._shard_tests(file_list, False)

        manager_connection = manager_worker_broker.get(self._port, self._options, self, worker.Worker)

        # FIXME: start all of the workers.
        manager_connection.start_worker(0)

        for test_list in test_lists:
            manager_connection.post_message('test_list', test_list[0], test_list[1])

        manager_connection.post_message('stop')

        keyboard_interrupted = False
        interrupted = False
        if not self._options.dry_run:
            while not self._check_if_done():
                manager_connection.run_message_loop(delay_secs=1.0)

        # FIXME: implement stats.
        thread_timings = []

        # FIXME: should this be a class instead of a tuple?
        return (keyboard_interrupted, interrupted, thread_timings,
                self._group_stats, self._all_results)

    def _check_if_done(self):
        """Returns true iff all the workers have either completed or wedged."""
        # FIXME: implement to check for wedged workers.
        return self._done

    def handle_started_test(self, src, test_info, hang_timeout):
        # FIXME: implement
        pass

    def handle_done(self, src):
        # FIXME: implement properly to handle multiple workers.
        self._done = True
        pass

    def handle_exception(self, src, exception_info):
        raise exception_info

    def handle_finished_list(self, src, list_name, num_tests, elapsed_time):
        # FIXME: update stats
        pass

    def handle_finished_test(self, src, result, elapsed_time):
        self._update_summary_with_result(self._current_result_summary, result)

        # FIXME: update stats.
        self._all_results.append(result)
