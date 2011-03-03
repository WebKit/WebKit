# Copyright (C) 2009 Google Inc. All rights reserved.
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

# FIXME: Implement the rest of the interface as needed for testing :).

# FIXME: Unify with tool/mocktool.MockExecutive.

from webkitpy.common.system import executive


class MockExecutive2(object):
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

    def run_command(self, arg_list, error_handler=None, return_exit_code=False,
                    decode_output=False, return_stderr=False):
        if self._exception:
            raise self._exception
        if return_exit_code:
            return self._exit_code
        if self._run_command_fn:
            return self._run_command_fn(arg_list)
        if self._exit_code and error_handler:
            script_error = executive.ScriptError(script_args=arg_list,
                                                 exit_code=self._exit_code,
                                                 output=self._output)
            error_handler(script_error)
        if return_stderr:
            return self._output + self._stderr
        return self._output
