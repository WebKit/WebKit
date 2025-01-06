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
    capture_output = kwargs.pop('capture_output', False)

    with Timeout.DisableAlarm():
        current_time = time.time()
        Timeout.check(current_time=current_time)
        difference = Timeout.difference(current_time=current_time)

        if difference:
            timeout = min(timeout or sys.maxsize, int(math.ceil(difference)))
        if capture_output:
            if ('stdout' in kwargs) or ('stderr' in kwargs):
                raise ValueError('stdout and stderr arguments may not be used with capture_output.')
            kwargs['stdout'] = subprocess.PIPE
            kwargs['stderr'] = subprocess.PIPE
        return subprocess.run(*popenargs, timeout=timeout, **kwargs)


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
