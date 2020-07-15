# Copyright (C) 2011 Google Inc. All rights reserved.
# Copyright (C) 2011-2019 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import sys
import time
import unittest

from webkitpy.port.factory import PortFactory
from webkitpy.port import server_process
from webkitpy.common.system.systemhost import SystemHost
from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.common.system.outputcapture import OutputCapture


class TrivialMockPort(object):
    def __init__(self):
        self.host = MockSystemHost()
        self.host.executive.kill_process = lambda x: None
        self.host.executive.kill_process = lambda x: None

    def architecture(self):
        return self.host.platform.architecture()

    def results_directory(self):
        return "/mock-results"

    def check_for_leaks(self, process_name, process_id):
        pass

    def process_kill_time(self):
        return 1


class MockFile(object):
    def __init__(self, server_process):
        self._server_process = server_process
        self.closed = False

    def fileno(self):
        return 1

    def write(self, line):
        self._server_process.broken_pipes.append(self)
        raise IOError

    def read(self, size=0):
        # This means end of file
        return ''

    def close(self):
        self.closed = True


class MockProc(object):
    def __init__(self, server_process):
        self.stdin = MockFile(server_process)
        self.stdout = MockFile(server_process)
        self.stderr = MockFile(server_process)
        self.pid = 1

    def poll(self):
        return 1

    def wait(self):
        return 0


class FakeServerProcess(server_process.ServerProcess):
    def _start(self):
        self._proc = MockProc(self)
        self.stdin = self._proc.stdin
        self.stdout = self._proc.stdout
        self.stderr = self._proc.stderr
        self._pid = self._proc.pid
        self.broken_pipes = []


class TestServerProcess(unittest.TestCase):
    stderr_print = 'print >>sys.stderr, "stderr"' if sys.version_info < (3, 0) else 'print("stderr", file=sys.stderr)'

    def serial_test_basic(self):
        # Give -u switch to force stdout and stderr to be unbuffered for Windows
        cmd = [sys.executable, '-uc', 'import sys; print("stdout"); print("again"); {}; sys.stdin.readline();'.format(self.stderr_print)]
        host = SystemHost()
        factory = PortFactory(host)
        port = factory.get()
        now = time.time()
        proc = server_process.ServerProcess(port, 'python', cmd)
        proc.write(b'')

        self.assertEqual(proc.poll(), None)
        self.assertFalse(proc.has_crashed())

        # check that doing a read after an expired deadline returns
        # nothing immediately.
        line = proc.read_stdout_line(now - 1)
        self.assertEqual(line, None)

        line = proc.read_stdout_line(now + 1.0)
        self.assertEqual(line.strip(), b"stdout")

        self.assertTrue(proc.has_available_stdout())

        line = proc.read_stderr_line(now + 1.0)
        self.assertEqual(line.strip(), b"stderr")

        line = proc.read_stdout_line(now + 1.0)
        self.assertEqual(line.strip(), b"again")
        self.assertFalse(proc.has_available_stdout())

        proc.write(b'End\n')
        time.sleep(0.1)  # Give process a moment to close.
        self.assertEqual(proc.poll(), 0)

        proc.stop(0)

    def serial_test_read_after_process_exits(self):
        cmd = [sys.executable, '-uc', 'import sys; print("stdout"); {};'.format(self.stderr_print)]
        host = SystemHost()
        factory = PortFactory(host)
        port = factory.get()
        now = time.time()
        proc = server_process.ServerProcess(port, 'python', cmd)
        proc.write(b'')
        time.sleep(0.1)  # Give process a moment to close.

        line = proc.read_stdout_line(now + 1.0)
        self.assertEqual(line.strip(), b"stdout")

        line = proc.read_stderr_line(now + 1.0)
        self.assertEqual(line.strip(), b"stderr")

        proc.stop(0)

    def serial_test_process_crashing(self):
        # Give -u switch to force stdout to be unbuffered for Windows
        cmd = [sys.executable, '-uc', 'import sys; print("stdout 1"); print("stdout 2"); print("stdout 3"); sys.stdin.readline(); sys.exit(1);']
        host = SystemHost()
        factory = PortFactory(host)
        port = factory.get()
        now = time.time()
        proc = server_process.ServerProcess(port, 'python', cmd)
        proc.write(b'')

        line = proc.read_stdout_line(now + 1.0)
        self.assertEqual(line.strip(), b'stdout 1')

        proc.write(b'End\n')
        time.sleep(0.1)  # Give process a moment to close.

        line = proc.read_stdout_line(now + 1.0)
        self.assertEqual(line.strip(), b'stdout 2')

        self.assertEqual(True, proc.has_crashed())

        line = proc.read_stdout_line(now + 1.0)
        self.assertEqual(line, None)

        proc.stop(0)

    def serial_test_process_crashing_no_data(self):
        cmd = [sys.executable, '-uc', 'import sys; sys.stdin.readline(); sys.exit(1);']
        host = SystemHost()
        factory = PortFactory(host)
        port = factory.get()
        now = time.time()
        proc = server_process.ServerProcess(port, 'python', cmd)
        proc.write(b'')

        self.assertEqual(False, proc.has_crashed())

        proc.write(b'End\n')
        time.sleep(0.1)  # Give process a moment to close.

        line = proc.read_stdout_line(now + 1.0)
        self.assertEqual(line, None)

        self.assertEqual(True, proc.has_crashed())

        proc.stop(0)

    def test_cleanup(self):
        port_obj = TrivialMockPort()
        server_process = FakeServerProcess(port_obj=port_obj, name="test", cmd=["test"])
        server_process._start()
        server_process.stop()
        self.assertTrue(server_process.stdin.closed)
        self.assertTrue(server_process.stdout.closed)
        self.assertTrue(server_process.stderr.closed)

    def test_broken_pipe(self):
        port_obj = TrivialMockPort()

        port_obj.host.platform.os_name = 'win'
        server_process = FakeServerProcess(port_obj=port_obj, name="test", cmd=["test"])
        server_process.write("should break")
        self.assertTrue(server_process.has_crashed())
        self.assertIsNotNone(server_process.pid())
        self.assertIsNone(server_process._proc)
        self.assertEqual(server_process.broken_pipes, [server_process.stdin])

        port_obj.host.platform.os_name = 'mac'
        server_process = FakeServerProcess(port_obj=port_obj, name="test", cmd=["test"])
        server_process.write("should break")
        self.assertTrue(server_process.has_crashed())
        self.assertIsNone(server_process._proc)
        self.assertEqual(server_process.broken_pipes, [server_process.stdin])
