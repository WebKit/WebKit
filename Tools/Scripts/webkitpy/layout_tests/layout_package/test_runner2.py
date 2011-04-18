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
import time

from webkitpy.tool import grammar

from webkitpy.layout_tests.layout_package import manager_worker_broker
from webkitpy.layout_tests.layout_package import test_runner
from webkitpy.layout_tests.layout_package import worker


_log = logging.getLogger(__name__)


class _WorkerState(object):
    """A class for the TestRunner/manager to use to track the current state
    of the workers."""
    def __init__(self, number, worker_connection):
        self.worker_connection = worker_connection
        self.number = number
        self.done = False
        self.current_test_name = None
        self.next_timeout = None
        self.wedged = False
        self.stats = {}
        self.stats['name'] = worker_connection.name
        self.stats['num_tests'] = 0
        self.stats['total_time'] = 0

    def __repr__(self):
        return "_WorkerState(" + str(self.__dict__) + ")"


class TestRunner2(test_runner.TestRunner):
    def __init__(self, port, options, printer):
        test_runner.TestRunner.__init__(self, port, options, printer)
        self._all_results = []
        self._group_stats = {}
        self._current_result_summary = None

        # This maps worker names to the state we are tracking for each of them.
        self._worker_states = {}

    def is_done(self):
        worker_states = self._worker_states.values()
        return worker_states and all(self._worker_is_done(worker_state) for worker_state in worker_states)

    def _worker_is_done(self, worker_state):
        t = time.time()
        if worker_state.done or worker_state.wedged:
            return True

        next_timeout = worker_state.next_timeout
        WEDGE_PADDING = 40.0
        if next_timeout and t > next_timeout + WEDGE_PADDING:
            _log.error('')
            worker_state.worker_connection.log_wedged_worker(worker_state.current_test_name)
            _log.error('')
            worker_state.wedged = True
            return True
        return False

    def name(self):
        return 'TestRunner2'

    def _run_tests(self, file_list, result_summary):
        """Runs the tests in the file_list.

        Return: A tuple (interrupted, keyboard_interrupted, thread_timings,
            test_timings, individual_test_timings)
            interrupted is whether the run was interrupted
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
        self._all_results = []
        self._group_stats = {}
        self._worker_states = {}

        keyboard_interrupted = False
        interrupted = False
        thread_timings = []

        self._printer.print_update('Sharding tests ...')
        test_lists = self._shard_tests(file_list,
            (int(self._options.child_processes) > 1) and not self._options.experimental_fully_parallel)

        num_workers = self._num_workers(len(test_lists))

        manager_connection = manager_worker_broker.get(self._port, self._options,
                                                       self, worker.Worker)

        if self._options.dry_run:
            return (keyboard_interrupted, interrupted, thread_timings,
                    self._group_stats, self._all_results)

        self._printer.print_update('Starting %s ...' %
                                   grammar.pluralize('worker', num_workers))
        for worker_number in xrange(num_workers):
            worker_connection = manager_connection.start_worker(worker_number)
            worker_state = _WorkerState(worker_number, worker_connection)
            self._worker_states[worker_connection.name] = worker_state

            # FIXME: If we start workers up too quickly, DumpRenderTree appears
            # to thrash on something and time out its first few tests. Until
            # we can figure out what's going on, sleep a bit in between
            # workers.
            time.sleep(0.1)

        self._printer.print_update("Starting testing ...")
        for test_list in test_lists:
            manager_connection.post_message('test_list', test_list[0], test_list[1])

        # We post one 'stop' message for each worker. Because the stop message
        # are sent after all of the tests, and because each worker will stop
        # reading messsages after receiving a stop, we can be sure each
        # worker will get a stop message and hence they will all shut down.
        for i in xrange(num_workers):
            manager_connection.post_message('stop')

        try:
            while not self.is_done():
                # We loop with a timeout in order to be able to detect wedged threads.
                manager_connection.run_message_loop(delay_secs=1.0)

            if any(worker_state.wedged for worker_state in self._worker_states.values()):
                _log.error('')
                _log.error('Remaining workers are wedged, bailing out.')
                _log.error('')
            else:
                _log.debug('No wedged threads')

            # Make sure all of the workers have shut down (if possible).
            for worker_state in self._worker_states.values():
                if not worker_state.wedged and worker_state.worker_connection.is_alive():
                    worker_state.worker_connection.join(0.5)
                    assert not worker_state.worker_connection.is_alive()

        except KeyboardInterrupt:
            _log.info("Interrupted, exiting")
            self.cancel_workers()
            keyboard_interrupted = True
        except test_runner.TestRunInterruptedException, e:
            _log.info(e.reason)
            self.cancel_workers()
            interrupted = True
        except:
            # Unexpected exception; don't try to clean up workers.
            _log.info("Exception raised, exiting")
            raise

        thread_timings = [worker_state.stats for worker_state in self._worker_states.values()]

        # FIXME: should this be a class instead of a tuple?
        return (interrupted, keyboard_interrupted, thread_timings,
                self._group_stats, self._all_results)

    def cancel_workers(self):
        for worker_state in self._worker_states.values():
            worker_state.worker_connection.cancel()

    def handle_started_test(self, source, test_info, hang_timeout):
        worker_state = self._worker_states[source]
        worker_state.current_test_name = self._port.relative_test_filename(test_info.filename)
        worker_state.next_timeout = time.time() + hang_timeout

    def handle_done(self, source):
        worker_state = self._worker_states[source]
        worker_state.done = True

    def handle_exception(self, source, exception_info):
        exception_type, exception_value, exception_traceback = exception_info
        raise exception_type, exception_value, exception_traceback

    def handle_finished_list(self, source, list_name, num_tests, elapsed_time):
        self._group_stats[list_name] = (num_tests, elapsed_time)

    def handle_finished_test(self, source, result, elapsed_time):
        worker_state = self._worker_states[source]
        worker_state.next_timeout = None
        worker_state.current_test_name = None
        worker_state.stats['total_time'] += elapsed_time
        worker_state.stats['num_tests'] += 1

        if worker_state.wedged:
            # This shouldn't happen if we have our timeouts tuned properly.
            _log.error("%s unwedged", source)

        self._all_results.append(result)
        self._update_summary_with_result(self._current_result_summary, result)
