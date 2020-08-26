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

import os
import io
import subprocess
import signal
import sys
import time

from webkitcorepy import log, string_utils, Timeout, TimeoutExpired, unicode
from webkitcorepy.mocks import Subprocess

# This file is mocked version of the subprocess.Popen object. This object differs slightly between Python 2 and 3.
# This object is not a complete mock of subprocess.Popen, but it does enable thorough testing of code which uses Popen,
# in particular, the subprocess.check_call, subprocess.check_output and subprocess.run commands.
#
# If you find yourself needing to edit this file, lean heavily on the testing in
# webkitcorepy.tests.mocks.subprocess_unittest and Python's implementation of Popen.


class PopenBase(object):
    NEXT_PID = os.getpid() + 1

    def __init__(self, args, bufsize=None, cwd=None, stdin=None, stdout=None, stderr=None):
        self._completion = None
        self._communication_started = False
        if bufsize is None:
            bufsize = -1
        if not isinstance(bufsize, int):
            raise TypeError("bufsize must be an integer")

        self._args = args
        self._cwd = cwd

        self.returncode = None

        self.stdin = string_utils.BytesIO() if stdin is None or stdin == -1 else stdin
        self.stdout = string_utils.BytesIO() if stdout == -1 else stdout
        self._stdout_type = bytes if stdout == -1 else str
        self.stderr = string_utils.BytesIO() if stderr == -1 else stderr
        self._stderr_type = bytes if stderr == -1 else str

        Subprocess.completion_generator_for(args[0])

        self.pid = self.NEXT_PID
        self.NEXT_PID += 1

        self._start_time = time.time()

    @property
    def universal_newlines(self):
        return self.text_mode

    @universal_newlines.setter
    def universal_newlines(self, universal_newlines):
        self.text_mode = bool(universal_newlines)

    def poll(self):
        if not self._completion:
            self.stdin.seek(0)
            self._completion = Subprocess.completion_for(*self._args, cwd=self._cwd, input=self.stdin.read())

            (self.stdout or sys.stdout).write(
                string_utils.decode(self._completion.stdout, target_type=self._stdout_type))
            (self.stdout or sys.stdout).flush()

            (self.stderr or sys.stderr).write(
                string_utils.decode(self._completion.stderr, target_type=self._stderr_type))
            (self.stderr or sys.stderr).flush()

        if self.returncode is not None and time.time() >= self._start_time + self._completion.elapsed:
            self.returncode = self._completion.returncode
            if self.stdout:
                self.stdout.seek(0)
            if self.stderr:
                self.stderr.seek(0)

        return self.returncode

    def send_signal(self, sig):
        if self.returncode is not None:
            return

        if sig not in [signal.SIGTERM, signal.SIGKILL]:
            raise ValueError('Mock Popen object cannot handle signal {}'.format(sig))
        log.critical('Mock process {} send signal {}'.format(self.pid, sig))
        self.returncode = -1

    def terminate(self):
        self.send_signal(signal.SIGTERM)

    def kill(self):
        self.send_signal(signal.SIGKILL)


if sys.version_info > (3, 0):
    class Popen(PopenBase):
        def __init__(self, args, bufsize=None, executable=None,
                     stdin=None, stdout=None, stderr=None,
                     preexec_fn=None, close_fds=True,
                     shell=False, cwd=None, env=None, universal_newlines=None,
                     startupinfo=None, creationflags=0,
                     restore_signals=True, start_new_session=False,
                     pass_fds=(), encoding=None, errors=None, text=None):

            super(Popen, self).__init__(args, bufsize=bufsize, cwd=cwd, stdin=stdin, stdout=stdout, stderr=stderr)

            if pass_fds and not close_fds:
                log.warn("pass_fds overriding close_fds.")

            for arg in args:
                if not isinstance(arg, (str, bytes, os.PathLike)):
                    raise TypeError(
                        'expected {}, {} or os.PathLike object, not {}',
                        str, bytes, type(arg),
                    )

            self.args = args
            self.encoding = encoding
            self.errors = errors

            if (text is not None and universal_newlines is not None and bool(universal_newlines) != bool(text)):
                raise subprocess.SubprocessError('Cannot disambiguate when both text and universal_newlines are supplied but different. Pass one or the other.')

            self.text_mode = encoding or errors or text or universal_newlines

            if self.stdin is not None and self.text_mode:
                self.stdin = io.TextIOWrapper(self.stdin, write_through=True, line_buffering=(bufsize == 1), encoding=encoding, errors=errors)
            if self.stdout is not None and self.text_mode:
                self.stdout = io.TextIOWrapper(self.stdout, encoding=encoding, errors=errors)
                self._stdout_type = str
            if self.stderr is not None and self.text_mode:
                self.stderr = io.TextIOWrapper(self.stderr, encoding=encoding, errors=errors)
                self._stderr_type = str

        def communicate(self, input=None, timeout=None):
            if self._communication_started and input:
                raise ValueError('Cannot send input after starting communication')

            self._communication_started = True
            if input:
                self.stdin.write(string_utils.encode(input))
            self.wait(timeout=timeout)
            return self.stdout.read() if self.stdout else None, self.stderr.read() if self.stderr else None

        def wait(self, timeout=None):
            if self.poll() is not None:
                return

            if timeout and (self._completion.elapsed is None or timeout < self._completion.elapsed):
                raise TimeoutExpired(self._args, timeout)

            if self._completion.elapsed is None:
                raise ValueError('Running a command that hangs without a timeout')

            if self._completion.elapsed:
                time.sleep(self._completion.elapsed)

            self.returncode = self._completion.returncode
            if self.stdout:
                self.stdout.seek(0)
            if self.stderr:
                self.stderr.seek(0)

        def __enter__(self):
            return self

        def __exit__(self, exc_type, exc_value, traceback):
            if self.stdout:
                self.stdout.close()
            if self.stderr:
                self.stderr.close()
            if self.stdin:
                self.stdin.close()

else:
    class Popen(PopenBase):
        def __init__(self, args, bufsize=-1, executable=None,
                     stdin=None, stdout=None, stderr=None,
                     preexec_fn=None, close_fds=True,
                     shell=False, cwd=None, env=None, universal_newlines=None,
                     startupinfo=None, creationflags=0):

            super(Popen, self).__init__(args, bufsize=bufsize, cwd=cwd, stdin=stdin, stdout=stdout, stderr=stderr)

            for index in range(len(args)):
                if not isinstance(args[index], (str, unicode)):
                    raise TypeError('execv() arg {} must contain only strings'.format(index + 1))

            self.text_mode = universal_newlines

        def communicate(self, input=None):
            if self._communication_started and input:
                raise ValueError('Cannot send input after starting communication')

            self._communication_started = True
            if input:
                self.stdin.write(input)
            self.wait()
            return self.stdout.read() if self.stdout else None, self.stderr.read() if self.stderr else None

        def wait(self):
            if self.poll() is not None:
                return

            # Need to check the timeout context
            timeout = Timeout.difference()
            if timeout and (self._completion.elapsed is None or timeout < self._completion.elapsed):
                raise TimeoutExpired(self._args, timeout)

            if self._completion.elapsed is None:
                raise ValueError('Running a command that hangs without a timeout')

            if self._completion.elapsed:
                time.sleep(self._completion.elapsed)

            self.returncode = self._completion.returncode
            if self.stdout:
                self.stdout.seek(0)
            if self.stderr:
                self.stderr.seek(0)
