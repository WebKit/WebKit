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

import sys
import time

from webkitpy.common.system.filesystem import FileSystem


class AbstractExecutive(object):

    def run_and_throw_if_fail(self, args, quiet=False, decode_output=True, **kwargs):
        raise NotImplementedError('subclasses must implement')

    def cpu_count(self):
        raise NotImplementedError('subclasses must implement')

    @staticmethod
    def interpreter_for_script(script_path, fs=None):
        fs = fs or FileSystem()
        lines = fs.read_text_file(script_path).splitlines()
        if not len(lines):
            return None
        first_line = lines[0]
        if not first_line.startswith('#!'):
            return None
        if first_line.find('python') > -1:
            return sys.executable
        if first_line.find('perl') > -1:
            return 'perl'
        if first_line.find('ruby') > -1:
            return 'ruby'
        return None

    @staticmethod
    def shell_command_for_script(script_path, fs=None):
        fs = fs or FileSystem()
        # Win32 does not support shebang. We need to detect the interpreter ourself.
        if sys.platform.startswith('win'):
            interpreter = AbstractExecutive.interpreter_for_script(script_path, fs)
            if interpreter:
                return [interpreter, script_path]
        return [script_path]

    def kill_process(self, pid):
        raise NotImplementedError('subclasses must implement')

    def check_running_pid(self, pid):
        raise NotImplementedError('subclasses must implement')

    def running_pids(self, process_name_filter=None):
        raise NotImplementedError('subclasses must implement')

    def wait_newest(self, process_name_filter=None):
        if not process_name_filter:
            process_name_filter = lambda process_name: True

        running_pids = self.running_pids(process_name_filter)
        if not running_pids:
            return
        pid = running_pids[-1]

        while self.check_running_pid(pid):
            time.sleep(0.25)

    def wait_limited(self, pid, limit_in_seconds=None, check_frequency_in_seconds=None):
        seconds_left = limit_in_seconds or 10
        sleep_length = check_frequency_in_seconds or 1
        while seconds_left > 0 and self.check_running_pid(pid):
            seconds_left -= sleep_length
            time.sleep(sleep_length)

    def interrupt(self, pid):
        raise NotImplementedError('subclasses must implement')

    @staticmethod
    def default_error_handler(error):
        raise error

    @staticmethod
    def ignore_error(error):
        pass

    def _stringify_args(self, args):
        return map(unicode, args)

    def command_for_printing(self, args):
        """Returns a print-ready string representing command args.
        The string should be copy/paste ready for execution in a shell."""
        args = self._stringify_args(args)
        escaped_args = []
        for arg in args:
            if isinstance(arg, unicode):
                # Escape any non-ascii characters for easy copy/paste
                arg = arg.encode("unicode_escape")
            # FIXME: Do we need to fix quotes here?
            escaped_args.append(arg)
        return " ".join(escaped_args)

    def run_command(self, args, cwd=None, env=None, input=None, error_handler=None, ignore_errors=False,
        return_exit_code=False, return_stderr=True, decode_output=True):
        raise NotImplementedError('subclasses must implement')

    def popen(self, args, **kwargs):
        raise NotImplementedError('subclasses must implement')

    def run_in_parallel(self, command_lines_and_cwds, processes=None):
        raise NotImplementedError('subclasses must implement')
