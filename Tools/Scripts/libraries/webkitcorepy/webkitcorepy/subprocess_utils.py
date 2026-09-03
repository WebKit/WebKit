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

import logging
import math
import os
import shlex
import subprocess
import sys
import threading
import time
import traceback
from subprocess import CompletedProcess, TimeoutExpired
from typing import Sequence, Union

from webkitcorepy import Timeout

__all__ = ['CompletedProcess', 'Thread', 'TimeoutExpired', 'join_subprocess_args', 'run']

log = logging.getLogger(__name__)


def join_subprocess_args(
    args: Union[str, bytes, os.PathLike, Sequence[Union[str, bytes, os.PathLike]]],
) -> Sequence[str]:
    """Runs shlex.join() for any valid subprocess.Popen args"""
    if isinstance(args, (str, bytes, os.PathLike)):
        args = [args]
    else:
        args = list(args)

    result = []
    for arg in args:
        if isinstance(arg, os.PathLike):
            arg = os.fspath(arg)

        if isinstance(arg, bytes):
            arg = os.fsdecode(arg)
        elif not isinstance(arg, str):
            raise TypeError(f'expected str, bytes, or PathLike, got {type(arg)}')

        result.append(arg)

    return shlex.join(result)


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

        args_for_log = join_subprocess_args(popenargs[0])

        try:
            proc = subprocess.run(*popenargs, timeout=timeout, **kwargs)
        except Exception:
            end_time = time.time()
            etype, value, _ = sys.exc_info()
            formatted_exception = ''.join(traceback.format_exception_only(etype, value)).strip('\n')
            log.debug(
                "Ran '%s', but got exception %s after %.2f s",
                args_for_log,
                formatted_exception,
                end_time - current_time,
            )
            raise
        else:
            end_time = time.time()
            log.debug(
                "Ran '%s' in %.2f s, exited with %d",
                args_for_log,
                end_time - current_time,
                proc.returncode,
            )

        return proc


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
