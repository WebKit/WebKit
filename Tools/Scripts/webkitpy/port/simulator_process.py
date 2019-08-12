# Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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


import os
import time

from webkitpy.common.timeout_context import Timeout
from webkitpy.port.server_process import ServerProcess


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
            if self._device.executive.check_running_pid(self.pid):
                self.returncode = None
            else:
                self.returncode = 1
            return self.returncode

        def wait(self):
            while not self.poll():
                time.sleep(0.01)  # In seconds
            return self.returncode

    # Python 2's implementation of makefile does not return a non-blocking file.
    class NonBlockingFileFromSocket(object):

        def __init__(self, sock, type):
            self.socket = sock
            self._file = os.fdopen(sock.fileno(), type, 0)
            ServerProcess._set_file_nonblocking(self._file)

        def __getattr__(self, name):
            return getattr(self._file, name)

        def close(self):
            result = self._file.close()
            self.socket.close()
            return result

    def __init__(self, port_obj, name, cmd, env=None, universal_newlines=False, treat_no_data_as_crash=False, target_host=None):
        env['PORT'] = str(target_host.listening_port())  # The target_host should be a device.
        super(SimulatorProcess, self).__init__(port_obj, name, cmd, env, universal_newlines, treat_no_data_as_crash, target_host)

        self._bundle_id = port_obj.app_identifier_from_bundle(cmd[0])

    def process_name(self):
        return self._port.app_executable_from_bundle(self._cmd[0])

    @staticmethod
    def _accept_connection_create_file(server, type):
        connection, address = server.accept()
        assert address[0] == '127.0.0.1'
        return SimulatorProcess.NonBlockingFileFromSocket(connection, type)

    def _start(self):
        if self._proc:
            raise ValueError('{} already running'.format(self._name))
        self._reset()

        # Each device has a listening socket intitilaized during the port's setup_test_run.
        # 3 client connections will be accepted for stdin, stdout and stderr in that order.
        self._target_host.listening_socket.listen(3)
        self._pid = self._target_host.launch_app(self._bundle_id, self._cmd[1:], env=self._env)
        self._system_pid = self._pid

        with Timeout(15, RuntimeError('Timed out waiting for pid {} to connect at port {}'.format(self._pid, self._target_host.listening_port()))):
            stdin = None
            stdout = None
            stderr = None
            try:
                # This order matches the client side connections in Tools/TestRunnerShared/IOSLayoutTestCommunication.cpp setUpIOSLayoutTestCommunication()
                stdin = SimulatorProcess._accept_connection_create_file(self._target_host.listening_socket, 'w')
                stdout = SimulatorProcess._accept_connection_create_file(self._target_host.listening_socket, 'rb')
                stderr = SimulatorProcess._accept_connection_create_file(self._target_host.listening_socket, 'rb')
            except:
                # We set self._proc as _reset() and _kill() depend on it.
                self._proc = SimulatorProcess.Popen(self._pid, stdin, stdout, stderr, self._target_host)
                if self._proc.poll() is not None:
                    self._reset()
                    raise Exception('App {} with pid {} crashed before stdin could be attached'.format(os.path.basename(self._cmd[0]), self._pid))
                self._kill()
                self._reset()
                raise

        self._proc = SimulatorProcess.Popen(self._pid, stdin, stdout, stderr, self._target_host)

    def stop(self, timeout_secs=3.0):
        # Only bother to check for leaks or stderr if the process is still running.
        if self.poll() is None:
            self._port.check_for_leaks(self.process_name(), self.pid())
            for child_process_name in self._child_processes.keys():
                for child_process_id in self._child_processes[child_process_name]:
                    self._port.check_for_leaks(child_process_name, child_process_id)

        if self._proc and self._proc.pid:
            self._target_host.executive.kill_process(self._proc.pid)

        return self._wait_for_stop(timeout_secs)
