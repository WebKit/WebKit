# Copyright (C) 2020 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
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

import subprocess
import sys
import time
import unittest

from webkitcorepy import mocks, OutputCapture, run, TimeoutExpired, Timeout, Thread
from webkitcorepy.subprocess_utils import run_all


class SubprocessUtils(unittest.TestCase):

    def test_run(self):
        result = run([sys.executable, '-c', 'print("message")'], capture_output=True, encoding='utf-8')
        self.assertEqual(0, result.returncode)
        self.assertEqual(result.stdout, 'message\n')
        self.assertEqual(result.stderr, '')

    def test_run_exit(self):
        result = run([sys.executable, '-c', 'import sys;sys.exit(1)'])
        self.assertEqual(1, result.returncode)
        self.assertEqual(result.stdout, None)
        self.assertEqual(result.stderr, None)

    def test_thread(self):
        data = dict()

        def f():
            data['finished'] = True

        t = Thread(target=f)
        with t:
            pass

        self.assertEqual(t.poll(), 0)
        self.assertTrue(data.get('finished', False))

    def test_killed_thread(self):
        data = dict()

        def f():
            data['iteration'] = 0
            for x in range(10):
                if Thread.terminated():
                    break
                data['iteration'] = x
                time.sleep(.1)

        t = Thread(target=f)
        with t:
            self.assertIsNone(t.poll())
            t.kill()

        self.assertEqual(t.poll(), 1)
        self.assertNotEqual(data.get('iteration', 9), 9)

    def test_run_timeout(self):
        with OutputCapture(), self.assertRaises(TimeoutExpired):
            run([sys.executable, '-c', 'import time;time.sleep(2)'], timeout=1)

    def test_run_timeout_context(self):
        with OutputCapture(), self.assertRaises(TimeoutExpired):
            with Timeout(1):
                run([sys.executable, '-c', 'import time;time.sleep(2)'])

    def test_input(self):
        def callback(*args, **kwargs):
            print(kwargs['input'])
            return mocks.ProcessCompletion(returncode=0)

        with OutputCapture() as captured, mocks.Subprocess('command', generator=callback):
            run(['command'], input=b'stdin content')

        self.assertEqual(captured.stdout.getvalue(), "b'stdin content'\n")

    def test_input_text(self):
        def callback(*args, **kwargs):
            print(kwargs['input'])
            return mocks.ProcessCompletion(returncode=0)

        with OutputCapture() as captured, mocks.Subprocess('command', generator=callback):
            run(['command'], input='stdin content', text=True)

        self.assertEqual(captured.stdout.getvalue(), "stdin content\n")


class RunAllTest(unittest.TestCase):

    def test_every_command_runs(self):
        results = run_all([[sys.executable, '-c', 'print({})'.format(n)] for n in range(4)], timeout=30)
        self.assertEqual([code for code, _, _ in results], [0] * 4)
        self.assertEqual([out.strip() for _, out, _ in results], [b'0', b'1', b'2', b'3'])

    def test_a_process_which_fills_its_pipe_does_not_stall_the_others(self):
        # Draining one at a time would leave the later ones blocked on a full stdout pipe.
        chatty = [sys.executable, '-c', 'print("x" * 200000)']
        results = run_all([chatty for _ in range(3)], timeout=30)
        self.assertEqual([code for code, _, _ in results], [0, 0, 0])
        for _code, out, _err in results:
            self.assertEqual(len(out.strip()), 200000)

    def test_results_line_up_with_the_commands(self):
        results = run_all([
            [sys.executable, '-c', 'print("first")'],
            [sys.executable, '-c', 'import sys; sys.exit(3)'],
            [sys.executable, '-c', 'print("third")'],
        ], timeout=30)
        self.assertEqual(results[0][1].strip(), b'first')
        self.assertEqual(results[1][0], 3)
        self.assertEqual(results[2][1].strip(), b'third')

    def test_a_command_which_cannot_start_kills_the_ones_which_did(self):
        started = []

        def popen(command, **kwargs):
            if len(started) == 2:
                raise OSError('cannot start')
            process = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(60)'], **kwargs)
            started.append(process)
            return process

        with self.assertRaises(OSError):
            run_all([['ignored']] * 3, timeout=30, popen=popen)

        self.assertEqual(len(started), 2)
        for process in started:
            self.assertIsNotNone(process.wait(timeout=10))
