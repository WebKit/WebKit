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


def wait_until_exit(process, timeout=None):
    """Waits for a process to exit, killing it if it overruns its timeout.

    Returns (returncode, stdout, stderr). A process which had to be killed reports a non-zero returncode. Waits on
    the process rather than through Timeout, so this is safe to call off the main thread."""
    try:
        stdout, stderr = process.communicate(timeout=timeout)
        return process.returncode, stdout, stderr
    except TimeoutExpired:
        process.kill()
        stdout, stderr = process.communicate()
        # A process killed for overrunning its timeout has not succeeded, even if it happened to exit 0 as we killed it.
        return process.returncode or 1, stdout, stderr


def run_all(commands, timeout=None, popen=None, **kwargs):
    """Starts every command before waiting on any of them, and returns each one's (returncode, stdout, stderr).

    Each process is drained by its own thread, because a process which fills its stdout pipe stays blocked until
    something reads it: waiting on them one after another would let the later ones stall while an earlier one is read.

    The whole set shares `timeout`. `popen` overrides how each command is started, which is how a caller with a mock
    executive keeps these off real subprocesses."""
    popen = popen or subprocess.Popen
    processes = []
    results = [None] * len(commands)

    try:
        for command in commands:
            processes.append(popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, **kwargs))
    except BaseException:
        # Commands already started would otherwise outlive the call which failed to start the rest.
        for process in processes:
            process.kill()
            process.communicate()
        raise

    def drain(index, process):
        results[index] = wait_until_exit(process, timeout=timeout)

    threads = [Thread(target=drain, args=(index, process), name='run-all-{}'.format(index))
               for index, process in enumerate(processes)]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()
    return results


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
