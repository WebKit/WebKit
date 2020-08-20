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

import math
import subprocess
import sys
import time

from webkitcorepy import Timeout, string_utils

if sys.version_info > (3, 0):
    TimeoutExpired = subprocess.TimeoutExpired
    CompletedProcess = subprocess.CompletedProcess

    # Allows native integration with the Timeout context
    def run(*popenargs, **kwargs):
        timeout = kwargs.pop('timeout', None)

        with Timeout.DisableAlarm():
            current_time = time.time()
            Timeout.check(current_time=current_time)
            difference = Timeout.difference(current_time=current_time)

            if difference:
                timeout = min(timeout or sys.maxsize, int(math.ceil(difference)))
            return subprocess.run(*popenargs, timeout=timeout, **kwargs)

else:
    class TimeoutExpired(Exception):
        def __init__(self, cmd, timeout, output=None, stderr=None):
            self.cmd = cmd
            self.timeout = timeout
            self.output = output
            self.stderr = stderr

        def __str__(self):
            return "Command '{}' timed out after {} seconds".format(self.cmd, self.timeout)

        @property
        def stdout(self):
            return self.output

        @stdout.setter
        def stdout(self, value):
            self.output = value

    class CompletedProcess(object):
        def __init__(self, args, returncode, stdout=None, stderr=None):
            self.args = args
            self.returncode = returncode
            self.stdout = stdout
            self.stderr = stderr

        def __repr__(self):
            args = ['args={!r}'.format(self.args), 'returncode={!r}'.format(self.returncode)]
            if self.stdout is not None:
                args.append('stdout={!r}'.format(self.stdout))
            if self.stderr is not None:
                args.append('stderr={!r}'.format(self.stderr))
            return "{}({})".format(type(self).__name__, ', '.join(args))

        def check_returncode(self):
            if self.returncode:
                raise subprocess.CalledProcessError(self.returncode, self.args, self.stdout)

    # Taken directly from Python 3's subprocess library, since the run(...) API is superior to Python 2's subprocess API
    def run(*popenargs, **kwargs):
        input = kwargs.pop('input', None)
        capture_output = kwargs.pop('capture_output', False)
        timeout = kwargs.pop('timeout', None)
        check = kwargs.pop('check', False)

        encoding = kwargs.pop('encoding', None)
        errors = kwargs.pop('errors', None)
        if encoding or errors:
            def decode(string):
                return string_utils.decode(string, encoding=encoding or 'utf-8', errors=errors or 'strict')
        else:
            def decode(string):
                return string

        if input is not None:
            if 'stdin' in kwargs:
                raise ValueError('stdin and input arguments may not both be used.')
            kwargs['stdin'] = subprocess.PIPE

        if capture_output:
            if ('stdout' in kwargs) or ('stderr' in kwargs):
                raise ValueError('stdout and stderr arguments may not be used with capture_output.')
            kwargs['stdout'] = subprocess.PIPE
            kwargs['stderr'] = subprocess.PIPE

        process = subprocess.Popen(*popenargs, **kwargs)
        try:
            if timeout:
                with Timeout(timeout):
                    stdout, stderr = process.communicate(input)
            else:
                stdout, stderr = process.communicate(input)

        except Timeout.Exception:
            process.kill()
            stdout, stderr = process.communicate()
            raise TimeoutExpired(popenargs[0], timeout, output=decode(stdout), stderr=decode(stderr))
        except BaseException:
            process.kill()
            raise
        finally:
            process.wait()

        retcode = process.poll()
        if check and retcode:
            raise subprocess.CalledProcessError(retcode, popenargs[0], output=decode(stdout), stderr=decode(stderr))
        return CompletedProcess(popenargs[0], retcode, decode(stdout), decode(stderr))
