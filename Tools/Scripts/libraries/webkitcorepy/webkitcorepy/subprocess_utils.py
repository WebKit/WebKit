# Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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
import os
import subprocess
import sys
import time
import threading

from webkitcorepy import Timeout

TimeoutExpired = subprocess.TimeoutExpired
CompletedProcess = subprocess.CompletedProcess


# Allows native integration with the Timeout context
def run(*popenargs, **kwargs):
    timeout = kwargs.pop('timeout', None)
    check = kwargs.pop('check', False)
    input = kwargs.pop('input', None)
    capture_output = kwargs.pop('capture_output', False)
    capture_and_stream_output = kwargs.pop('capture_and_stream_output', False)

    with Timeout.DisableAlarm():
        current_time = time.time()
        Timeout.check(current_time=current_time)
        difference = Timeout.difference(current_time=current_time)

        if difference:
            timeout = min(timeout or sys.maxsize, int(math.ceil(difference)))
        if input is not None:
            if kwargs.get('stdin') is not None:
                raise ValueError('stdin and input arguments may not both be used.')
            kwargs['stdin'] = subprocess.PIPE
        if capture_output or capture_and_stream_output:
            if ('stdout' in kwargs) or ('stderr' in kwargs):
                raise ValueError('stdout and stderr arguments may not be used with capture_output and capture_and_stream_output.')
            kwargs['stdout'] = subprocess.PIPE
            kwargs['stderr'] = subprocess.STDOUT if capture_and_stream_output else subprocess.PIPE

        # Mimic subprocess.run
        with subprocess.Popen(*popenargs, **kwargs) as process:
            output = []
            stdout = stderr = None
            try:
                if capture_and_stream_output:
                    for line in process.stdout:
                        sys.stdout.write(line)
                        output.append(line)
                stdout, stderr = process.communicate(input, timeout=timeout)
            except TimeoutExpired as exc:
                process.kill()
                if os.name == 'nt':
                    exc.stdout, exc.stderr = process.communicate()
                else:
                    process.wait()
                raise
            except:
                process.kill()
                raise
            retcode = process.poll()
            output_to_use = ''.join(output) or stdout
            if check and retcode:
                raise subprocess.CalledProcessError(retcode, process.args, output=output_to_use, stderr=stderr)
        return CompletedProcess(process.args, retcode, output_to_use, stderr)


class Thread(threading.Thread):
    @classmethod
    def terminated(cls):
        return getattr(threading.current_thread(), '_terminated', False)

    def __init__(self, *args, **kwargs):
        super(Thread, self).__init__(*args, **kwargs)
        self._terminated = False

    def poll(self):
        return None if self.is_alive() else {True: 1, False: 0}.get(self._terminated, -1)

    def terminate(self):
        self._terminated = True

    def kill(self):
        self._terminated = True

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, *args, **kwargs):
        with Timeout.DisableAlarm():
            current_time = time.time()
            Timeout.check(current_time=current_time)
            self.join(Timeout.difference(current_time=current_time))
