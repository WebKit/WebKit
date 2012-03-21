# Copyright (C) 2011 Google Inc. All rights reserved.
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
import unittest

from webkitpy.layout_tests.port import server_process
from webkitpy.common.system.executive import ScriptError
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.common.system.outputcapture import OutputCapture


def _logging_run_command(args):
    print args


def _throwing_run_command(args):
    raise ScriptError("MOCK script error")


class TrivialMockPort(object):
    def results_directory(self):
        return "/mock-results"

    def check_for_leaks(self, process_name, process_pid):
        pass

    def is_crash_reporter(self, process_name):
        return False


class MockFile(object):
    def __init__(self, server_process):
        self._server_process = server_process

    def fileno(self):
        return 1

    def write(self, line):
        self._server_process.broken_pipes.append(self)
        raise IOError

    def close(self):
        pass


class MockProc(object):
    def __init__(self, server_process):
        self.stdin = MockFile(server_process)
        self.stdout = MockFile(server_process)
        self.stderr = MockFile(server_process)
        self.pid = 1

    def poll(self):
        return 1


class FakeServerProcess(server_process.ServerProcess):
    def _start(self):
        self._proc = MockProc(self)
        self.stdin = self._proc.stdin
        self.broken_pipes = []


class TestServerProcess(unittest.TestCase):
    def test_broken_pipe(self):
        server_process = FakeServerProcess(port_obj=TrivialMockPort(), name="test", cmd=["test"])
        server_process.write("should break")
        self.assertTrue(server_process.crashed)
        self.assertEquals(server_process._proc, None)
        self.assertEquals(server_process.broken_pipes, [server_process.stdin])

    def test_sample_process(self):
        # Currently, sample-on-timeout only works on Darwin.
        if sys.platform != "darwin":
            return
        server_process = FakeServerProcess(port_obj=TrivialMockPort(), name="test", cmd=["test"], executive=MockExecutive2(run_command_fn=_logging_run_command))
        server_process._proc = MockProc(server_process)
        expected_stdout = "['/usr/bin/sample', 1, 10, 10, '-file', '/mock-results/test-1.sample.txt']\n"
        OutputCapture().assert_outputs(self, server_process._sample, expected_stdout=expected_stdout)

    def test_sample_process_throws_exception(self):
        # Currently, sample-on-timeout only works on Darwin.
        if sys.platform != "darwin":
            return
        server_process = FakeServerProcess(port_obj=TrivialMockPort(), name="test", cmd=["test"], executive=MockExecutive2(run_command_fn=_throwing_run_command))
        server_process._proc = MockProc(server_process)
        OutputCapture().assert_outputs(self, server_process._sample)
