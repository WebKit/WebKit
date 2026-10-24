# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import logging
import multiprocessing
import queue
import time

_log = logging.getLogger(__name__)

# The test process starts as a copy of this one, so run_file can be any callable.
_mp_context = multiprocessing.get_context('fork')


class TestProcess(object):
    """Runs test files with pytest in a separate process, one file at a time, and enforces a deadline for each subtest.

    When a subtest doesn't finish in time, the test process is killed, restart() is called (for example, to restart
    the WebDriver driver and the browser it controls), and the rest of the test file runs in a new test process.
    Nothing is interrupted inside the test process, so an asyncio event loop there can't be left in a bad state, and
    a hang in a fixture times out just like a hang in a test.
    """

    STOP_TIMEOUT_SECONDS = 10
    POLL_SECONDS = 1

    def __init__(self, run_file, timeout, restart=None):
        # run_file(path, deselected, report) runs in the test process. It runs the test file at path with
        # pytest_runner.run(), passing deselected and report on, and returns its harness result.
        # A timeout of 0 means no deadlines.
        self._run_file = run_file
        self._timeout = timeout
        self._restart = restart
        self._process = None
        self._tests = None
        self._events = None

    def start(self):
        # New queues every time, because a killed process can leave one in a bad state.
        self._tests = _mp_context.Queue()
        self._events = _mp_context.Queue()
        self._process = _mp_context.Process(target=TestProcess._main, args=(self._run_file, self._tests, self._events))
        self._process.start()

    def stop(self):
        if self._process is None:
            return
        self._tests.put(None)
        self._process.join(self.STOP_TIMEOUT_SECONDS)
        if self._process.is_alive():
            self._kill()
        self._process = None

    def run(self, path):
        results = []
        started = []
        while True:
            self._tests.put((path, list(started)))
            outcome = self._wait(results, started)
            if outcome[0] == 'done':
                return outcome[1], results

            _, subtest, phase, expected, timeout = outcome
            exit_code = self._process.exitcode
            self._kill()
            if exit_code is None:
                problem = 'Timeout (>%ss)' % timeout
                _log.error('%s in %s, restarting the test process' % (problem, subtest or path))
            else:
                problem = 'The test process exited with status %d' % exit_code
                _log.error('%s while running %s, restarting it' % (problem, subtest or path))
            if self._restart:
                self._restart()
            self.start()

            if subtest is None:
                return ('ERROR', '%s outside of any subtest' % problem), results
            results.append(self._result_for_unfinished_subtest(subtest, phase, expected, problem, exit_code is None))

    @staticmethod
    def _main(run_file, tests, events):
        while True:
            test = tests.get()
            if test is None:
                return
            path, deselected = test
            events.put(('done', run_file(path, deselected, events.put)))

    def _wait(self, results, started):
        subtest = phase = expected = None
        timeout = self._timeout
        deadline = time.monotonic() + timeout if timeout else None
        while True:
            # Wake up regularly to notice if the test process exited.
            wait = self.POLL_SECONDS if deadline is None else max(0, min(self.POLL_SECONDS, deadline - time.monotonic()))
            try:
                event = self._events.get(timeout=wait)
            except queue.Empty:
                if not self._process.is_alive() or (deadline is not None and time.monotonic() >= deadline):
                    return ('unfinished', subtest, phase, expected, timeout)
                continue

            kind = event[0]
            if kind == 'start':
                _, subtest, timeout, expected = event
                phase = None
                started.append(subtest)
            elif kind == 'phase':
                phase = event[1]
                continue
            elif kind == 'result':
                results.append(event[1])
                continue
            elif kind == 'finish':
                subtest = None
                timeout = self._timeout
            elif kind == 'done':
                return ('done', event[1])
            deadline = time.monotonic() + timeout if timeout else None

    def _kill(self):
        self._process.kill()
        self._process.join()

    @staticmethod
    def _result_for_unfinished_subtest(subtest, phase, expected, problem, timed_out):
        # The same results as when pytest-timeout interrupts a subtest: a timeout in setup or teardown is an error.
        if phase in (None, 'setup', 'teardown') or not timed_out:
            return (subtest, 'ERROR', '%s error' % (phase or 'setup'), problem)
        if 'TIMEOUT' in expected:
            return (subtest, 'XFAIL', None, None)
        return (subtest, 'TIMEOUT', None, problem)
