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

import os
import StringIO

from webkitpy.common.system.deprecated_logging import log
from webkitpy.common.system.executive import ScriptError


class MockProcess(object):
    def __init__(self, stdout='MOCK STDOUT\n', stderr=''):
        self.pid = 42
        self.stdout = StringIO.StringIO(stdout)
        self.stderr = StringIO.StringIO(stderr)
        self.stdin = StringIO.StringIO()
        self.returncode = 0

    def wait(self):
        return

# FIXME: This should be unified with MockExecutive2
class MockExecutive(object):
    PIPE = "MOCK PIPE"
    STDOUT = "MOCK STDOUT"

    @staticmethod
    def ignore_error(error):
        pass

    def __init__(self, should_log=False, should_throw=False, should_throw_when_run=None):
        self._should_log = should_log
        self._should_throw = should_throw
        self._should_throw_when_run = should_throw_when_run or set()
        # FIXME: Once executive wraps os.getpid() we can just use a static pid for "this" process.
        self._running_pids = {'test-webkitpy': os.getpid()}
        self._proc = None

    def check_running_pid(self, pid):
        return pid in self._running_pids.values()

    def running_pids(self, process_name_filter):
        running_pids = []
        for process_name, process_pid in self._running_pids.iteritems():
            if process_name_filter(process_name):
                running_pids.append(process_pid)

        log("MOCK running_pids: %s" % running_pids)
        return running_pids

    def run_and_throw_if_fail(self, args, quiet=False, cwd=None, env=None):
        if self._should_log:
            env_string = ""
            if env:
                env_string = ", env=%s" % env
            log("MOCK run_and_throw_if_fail: %s, cwd=%s%s" % (args, cwd, env_string))
        if self._should_throw_when_run.intersection(args):
            raise ScriptError("Exception for %s" % args, output="MOCK command output")
        return "MOCK output of child process"

    def run_command(self,
                    args,
                    cwd=None,
                    input=None,
                    error_handler=None,
                    return_exit_code=False,
                    return_stderr=True,
                    decode_output=False,
                    env=None):
        assert(isinstance(args, list) or isinstance(args, tuple))
        if self._should_log:
            env_string = ""
            if env:
                env_string = ", env=%s" % env
            input_string = ""
            if input:
                input_string = ", input=%s" % input
            log("MOCK run_command: %s, cwd=%s%s%s" % (args, cwd, env_string, input_string))
        output = "MOCK output of child process"
        if self._should_throw:
            raise ScriptError("MOCK ScriptError", output=output)
        return output

    def cpu_count(self):
        return 2

    def popen(self, args, cwd=None, env=None, **kwargs):
        if self._should_log:
            cwd_string = ""
            if cwd:
                cwd_string = ", cwd=%s" % cwd
            env_string = ""
            if env:
                env_string = ", env=%s" % env
            log("MOCK popen: %s%s%s" % (args, cwd_string, env_string))
        if not self._proc:
            self._proc = MockProcess()
        return self._proc

    def run_in_parallel(self, commands):
        command_outputs = []
        for cmd_line, cwd in commands:
            command_outputs.append([0, self.run_command(cmd_line, cwd=cwd), ''])
        return command_outputs

class MockExecutive2(object):
    @staticmethod
    def ignore_error(error):
        pass

    def __init__(self, output='', exit_code=0, exception=None,
                 run_command_fn=None, stderr=''):
        self._output = output
        self._stderr = stderr
        self._exit_code = exit_code
        self._exception = exception
        self._run_command_fn = run_command_fn

    def cpu_count(self):
        return 2

    def kill_all(self, process_name):
        pass

    def kill_process(self, pid):
        pass

    def run_command(self,
                    args,
                    cwd=None,
                    input=None,
                    error_handler=None,
                    return_exit_code=False,
                    return_stderr=True,
                    decode_output=False,
                    env=None):
        assert(isinstance(args, list) or isinstance(args, tuple))
        if self._exception:
            raise self._exception
        if self._run_command_fn:
            return self._run_command_fn(args)
        if return_exit_code:
            return self._exit_code
        if self._exit_code and error_handler:
            script_error = ScriptError(script_args=args, exit_code=self._exit_code, output=self._output)
            error_handler(script_error)
        if return_stderr:
            return self._output + self._stderr
        return self._output
