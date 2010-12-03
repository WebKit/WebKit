# Copyright (C) 2010 Google Inc. All rights reserved.
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

"""Module for handling messages and concurrency for run-webkit-tests.

Testing is accomplished by having a manager (TestRunner) gather all of the
tests to be run, and sending messages to a pool of workers (TestShellThreads)
to run each test. Each worker communicates with one driver (usually
DumpRenderTree) to run one test at a time and then compare the output against
what we expected to get.

This modules provides a message broker that connects the manager to the
workers: it provides a messaging abstraction and message loops, and
handles launching threads and/or processes depending on the
requested configuration.
"""

import logging
import sys
import time
import traceback

import dump_render_tree_thread

_log = logging.getLogger(__name__)


def get(port, options):
    """Return an instance of a WorkerMessageBroker."""
    worker_model = options.worker_model
    if worker_model == 'inline':
        return _InlineBroker(port, options)
    if worker_model == 'threads':
        return _MultiThreadedBroker(port, options)
    raise ValueError('unsupported value for --worker-model: %s' % worker_model)


class _WorkerMessageBroker(object):
    def __init__(self, port, options):
        self._port = port
        self._options = options

        # This maps worker_names to TestShellThreads
        self._threads = {}

    def start_worker(self, test_runner, worker_number):
        """Start a worker with the given index number.

        Returns the actual TestShellThread object."""
        # FIXME: Remove dependencies on test_runner.
        # FIXME: Replace with something that isn't a thread, and return
        # the name of the worker, not the thread itself. We need to return
        # the thread itself for now to allow TestRunner to access the object
        # directly to read shared state.
        thread = dump_render_tree_thread.TestShellThread(self._port,
            self._options, worker_number, test_runner._current_filename_queue,
            test_runner._result_queue)
        self._threads[thread.name()] = thread
        # Note: Don't start() the thread! If we did, it would actually
        # create another thread and start executing it, and we'd no longer
        # be single-threaded.
        return thread

    def cancel_worker(self, worker_name):
        """Attempt to cancel a worker (best-effort). The worker may still be
        running after this call returns."""
        self._threads[worker_name].cancel()

    def log_wedged_worker(self, worker_name):
        """Log information about the given worker's state."""
        raise NotImplementedError

    def run_message_loop(self, test_runner):
        """Loop processing messages until done."""
        # FIXME: eventually we'll need a message loop that the workers
        # can also call.
        raise NotImplementedError


class _InlineBroker(_WorkerMessageBroker):
    def run_message_loop(self, test_runner):
        thread = self._threads.values()[0]
        thread.run_in_main_thread(test_runner,
                                  test_runner._current_result_summary)

    def log_wedged_worker(self, worker_name):
        raise AssertionError('_InlineBroker.log_wedged_worker() called')


class _MultiThreadedBroker(_WorkerMessageBroker):
    def start_worker(self, test_runner, worker_number):
        thread = _WorkerMessageBroker.start_worker(self, test_runner,
                                                   worker_number)
        # Unlike the base implementation, here we actually want to start
        # the thread.
        thread.start()
        return thread

    def run_message_loop(self, test_runner):
        # FIXME: Remove the dependencies on test_runner. Checking on workers
        # should be done via a timer firing.
        test_runner._check_on_workers()

    def log_wedged_worker(self, worker_name):
        thread = self._threads[worker_name]
        stack = self._find_thread_stack(thread.id())
        assert(stack is not None)
        _log.error("")
        _log.error("%s (tid %d) is wedged" % (worker_name, thread.id()))
        self._log_stack(stack)
        _log.error("")

    def _find_thread_stack(self, id):
        """Returns a stack object that can be used to dump a stack trace for
        the given thread id (or None if the id is not found)."""
        for thread_id, stack in sys._current_frames().items():
            if thread_id == id:
                return stack
        return None

    def _log_stack(self, stack):
        """Log a stack trace to log.error()."""
        for filename, lineno, name, line in traceback.extract_stack(stack):
            _log.error('File: "%s", line %d, in %s' % (filename, lineno, name))
            if line:
                _log.error('  %s' % line.strip())
