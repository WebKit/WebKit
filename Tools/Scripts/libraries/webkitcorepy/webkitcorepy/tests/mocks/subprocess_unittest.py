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
import unittest

from webkitcorepy import BytesIO, OutputCapture, mocks, run


class MockSubprocess(unittest.TestCase):
    LS = mocks.Subprocess.Route(
        'ls',
        completion=mocks.ProcessCompletion(returncode=0, stdout='file1.txt\nfile2.txt\n'),
    )
    SLEEP = mocks.Subprocess.Route(
        'sleep',
        generator=lambda *args, **kwargs: mocks.ProcessCompletion(returncode=0, elapsed=int(args[1])),
    )

    def test_documentation(self):
        with OutputCapture():
            with mocks.Subprocess(
                    'ls', completion=mocks.ProcessCompletion(returncode=0, stdout='file1.txt\nfile2.txt\n'),
            ):
                result = run(['ls'], capture_output=True, encoding='utf-8')
                assert result.returncode == 0
                assert result.stdout == 'file1.txt\nfile2.txt\n'

            with mocks.Subprocess(
                    'ls', completion=mocks.ProcessCompletion(returncode=0, stdout='file1.txt\nfile2.txt\n'),
            ):
                assert subprocess.check_output(['ls']) == b'file1.txt\nfile2.txt\n'
                assert subprocess.check_call(['ls']) == 0

            with mocks.Subprocess(
                    mocks.Subprocess.CommandRoute('command-a', 'argument', completion=mocks.ProcessCompletion(returncode=0)),
                    mocks.Subprocess.CommandRoute('command-b', completion=mocks.ProcessCompletion(returncode=-1)),
            ):
                result = run(['command-a', 'argument'])
                assert result.returncode == 0

                result = run(['command-b'])
                assert result.returncode == -1

    def test_no_file(self):
        with mocks.Subprocess(self.LS, self.SLEEP):
            with self.assertRaises(OSError):
                run(['invalid-file'])

    def test_implied_route(self):
        with mocks.Subprocess('command', completion=mocks.ProcessCompletion(returncode=0)):
            self.assertEqual(run(['command']).returncode, 0)

            with self.assertRaises(OSError):
                run(['invalid-file'])

    def test_popen(self):
        with mocks.Time:
            with mocks.Subprocess(self.LS, self.SLEEP):
                ls = subprocess.Popen(['ls'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                ls.wait()
                self.assertEqual(0, ls.poll())
                self.assertEqual(b'file1.txt\nfile2.txt\n', ls.stdout.read())

                sleep = subprocess.Popen(['sleep', '1'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                self.assertEqual(None, sleep.poll())
                sleep.wait()
                self.assertEqual(0, sleep.poll())

    def test_ordered(self):
        with OutputCapture(), mocks.Subprocess(
            mocks.Subprocess.Route('command', completion=mocks.ProcessCompletion(returncode=0)),
            mocks.Subprocess.Route('command', completion=mocks.ProcessCompletion(returncode=1)),
            ordered=True,
        ):
            self.assertEqual(run(['command']).returncode, 0)
            self.assertEqual(run(['command']).returncode, 1)
            with self.assertRaises(OSError):
                run(['command'])

    def test_argument_priority(self):
        with OutputCapture(), mocks.Subprocess(
            mocks.Subprocess.Route('command', '--help', completion=mocks.ProcessCompletion(returncode=0)),
            mocks.Subprocess.Route('command', completion=mocks.ProcessCompletion(returncode=1)),
        ):
            self.assertEqual(run(['command']).returncode, 1)
            self.assertEqual(run(['command', '--help']).returncode, 0)

    def test_cwd_priority(self):
        with OutputCapture(), mocks.Subprocess(
            mocks.Subprocess.Route('command', completion=mocks.ProcessCompletion(returncode=0), cwd='/example'),
            mocks.Subprocess.Route('command', completion=mocks.ProcessCompletion(returncode=1)),
        ):
            self.assertEqual(run(['command']).returncode, 1)
            self.assertEqual(run(['command'], cwd='/example').returncode, 0)

    def test_input_priority(self):
        with OutputCapture(), mocks.Subprocess(
            mocks.Subprocess.Route('command', completion=mocks.ProcessCompletion(returncode=0), input='stdin'),
            mocks.Subprocess.Route('command', completion=mocks.ProcessCompletion(returncode=1)),
        ):
            self.assertEqual(run(['command']).returncode, 1)
            self.assertEqual(run(['command'], input='stdin').returncode, 0)
            self.assertEqual(run(['command'], stdin=BytesIO(b'stdin')).returncode, 0)


class MockCheckOutput(unittest.TestCase):
    def test_popen(self):
        with mocks.Subprocess(MockSubprocess.LS):
            result = subprocess.check_output(['ls'])
            self.assertEqual(result, b'file1.txt\nfile2.txt\n')


class MockCheckCall(unittest.TestCase):
    def test_popen(self):
        with OutputCapture() as captured:
            with mocks.Subprocess(MockSubprocess.LS):
                result = subprocess.check_call(['ls'])
                self.assertEqual(result, 0)

        self.assertEqual(captured.stdout.getvalue(), 'file1.txt\nfile2.txt\n')


class MockRun(unittest.TestCase):
    def test_popen(self):
        with OutputCapture() as captured:
            with mocks.Subprocess(MockSubprocess.LS):
                result = run(['ls'])
                self.assertEqual(result.returncode, 0)
                self.assertEqual(result.stdout, None)
                self.assertEqual(result.stderr, None)

        self.assertEqual(captured.stdout.getvalue(), 'file1.txt\nfile2.txt\n')
