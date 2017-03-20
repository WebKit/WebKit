# Copyright (C) 2017 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


import errno
import os
import signal
import time

from webkitpy.port.server_process import ServerProcess
from webkitpy.xcode.simulator import Simulator


class SimulatorProcess(ServerProcess):

    class Popen(object):

        def __init__(self, pid, stdin, stdout, stderr, device):
            self.stdin = stdin
            self.stdout = stdout
            self.stderr = stderr
            self.pid = pid
            self.returncode = None
            self._device = device

        def poll(self):
            if self.returncode:
                return self.returncode
            self.returncode = self._device.poll(self.pid)
            return self.returncode

        def wait(self):
            while not self.poll():
                time.sleep(0.01)  # In seconds
            return self.returncode

    def __init__(self, port_obj, name, cmd, env=None, universal_newlines=False, treat_no_data_as_crash=False, worker_number=None):
        self._bundle_id = port_obj.app_identifier_from_bundle(cmd[0])
        self._device = port_obj.device_for_worker_number(worker_number)
        env['IPC_IDENTIFIER'] = self._bundle_id + '-' + self._device.udid

        # This location matches the location used by WebKitTestRunner and DumpRenderTree
        # for the other side of these fifos.
        file_location = '/tmp/' + env['IPC_IDENTIFIER']
        self._in_path = file_location + '_IN'
        self._out_path = file_location + '_OUT'
        self._error_path = file_location + '_ERROR'

        super(SimulatorProcess, self).__init__(port_obj, name, cmd, env, universal_newlines, treat_no_data_as_crash)

    def _reset(self):
        super(SimulatorProcess, self)._reset()

        # Unlinks are needed on reset in the event that the Python code unexpectedly
        # fails between _start() and kill().  This can be caused by a SIGKILL or a crash.
        # This ensures that os.mkfifo() will not be obstructed by previous fifos.
        # Other files will still cause os.mkfifo() to fail.
        try:
            os.unlink(self._in_path)
        except:
            pass
        try:
            os.unlink(self._out_path)
        except:
            pass
        try:
            os.unlink(self._error_path)
        except:
            pass

    def _start(self):
        if self._proc:
            raise ValueError('{} already running'.format(self._name))
        self._reset()

        FIFO_PERMISSION_FLAGS = 0600  # Only owner can read and write
        os.mkfifo(self._in_path, FIFO_PERMISSION_FLAGS)
        os.mkfifo(self._out_path, FIFO_PERMISSION_FLAGS)
        os.mkfifo(self._error_path, FIFO_PERMISSION_FLAGS)

        stdout = os.fdopen(os.open(self._out_path, os.O_RDONLY | os.O_NONBLOCK), 'rb')
        stderr = os.fdopen(os.open(self._error_path, os.O_RDONLY | os.O_NONBLOCK), 'rb')

        self._pid = self._device.launch_app(self._bundle_id, self._cmd[1:], env=self._env)

        def handler(signum, frame):
            assert signum == signal.SIGALRM
            raise Exception('Timed out waiting for process to open {}'.format(self._in_path))
        signal.signal(signal.SIGALRM, handler)
        signal.alarm(3)  # In seconds

        stdin = None
        try:
            stdin = open(self._in_path, 'w', 0)  # Opening with no buffering, like popen
        except:
            # We set self._proc as _reset() and _kill() depend on it.
            self._proc = SimulatorProcess.Popen(self._pid, stdin, stdout, stderr, self._device)
            if self._proc.poll() is not None:
                self._reset()
                raise Exception('App {} crashed before stdin could be attached'.format(os.path.basename(self._cmd[0])))
            self._kill()
            self._reset()
            raise
        signal.alarm(0)  # Cancel alarm

        self._proc = SimulatorProcess.Popen(self._pid, stdin, stdout, stderr, self._device)

    def stop(self, timeout_secs=3.0):
        try:
            os.kill(self._pid, signal.SIGTERM)
        except OSError as err:
            assert err.errno == errno.ESRCH
            pass
        return super(SimulatorProcess, self).stop(timeout_secs)
