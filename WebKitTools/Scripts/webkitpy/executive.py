# Copyright (c) 2009, Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
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
import subprocess
import sys

from webkitpy.webkit_logging import tee


class ScriptError(Exception):

    def __init__(self,
                 message=None,
                 script_args=None,
                 exit_code=None,
                 output=None,
                 cwd=None):
        if not message:
            message = 'Failed to run "%s"' % script_args
            if exit_code:
                message += " exit_code: %d" % exit_code
            if cwd:
                message += " cwd: %s" % cwd

        Exception.__init__(self, message)
        self.script_args = script_args # 'args' is already used by Exception
        self.exit_code = exit_code
        self.output = output
        self.cwd = cwd

    def message_with_output(self, output_limit=500):
        if self.output:
            if output_limit and len(self.output) > output_limit:
                return "%s\nLast %s characters of output:\n%s" % \
                    (self, output_limit, self.output[-output_limit:])
            return "%s\n%s" % (self, self.output)
        return str(self)

    def command_name(self):
        command_path = self.script_args
        if type(command_path) is list:
            command_path = command_path[0]
        return os.path.basename(command_path)


def run_command(*args, **kwargs):
    # FIXME: This should not be a global static.
    # New code should use Executive.run_command directly instead
    return Executive().run_command(*args, **kwargs)


class Executive(object):

    def _run_command_with_teed_output(self, args, teed_output):
        child_process = subprocess.Popen(args,
                                         stdout=subprocess.PIPE,
                                         stderr=subprocess.STDOUT)

        # Use our own custom wait loop because Popen ignores a tee'd
        # stderr/stdout.
        # FIXME: This could be improved not to flatten output to stdout.
        while True:
            output_line = child_process.stdout.readline()
            if output_line == "" and child_process.poll() != None:
                return child_process.poll()
            teed_output.write(output_line)

    def run_and_throw_if_fail(self, args, quiet=False):
        # Cache the child's output locally so it can be used for error reports.
        child_out_file = StringIO.StringIO()
        if quiet:
            dev_null = open(os.devnull, "w")
        child_stdout = tee(child_out_file, dev_null if quiet else sys.stdout)
        exit_code = self._run_command_with_teed_output(args, child_stdout)
        if quiet:
            dev_null.close()

        child_output = child_out_file.getvalue()
        child_out_file.close()

        if exit_code:
            raise ScriptError(script_args=args,
                              exit_code=exit_code,
                              output=child_output)

    @staticmethod
    def cpu_count():
        # This API exists only in Python 2.6 and higher.  :(
        try:
            import multiprocessing
            return multiprocessing.cpu_count()
        except (ImportError, NotImplementedError):
            # This quantity is a lie but probably a reasonable guess for modern
            # machines.
            return 2

    # Error handlers do not need to be static methods once all callers are
    # updated to use an Executive object.

    @staticmethod
    def default_error_handler(error):
        raise error

    @staticmethod
    def ignore_error(error):
        pass

    # FIXME: This should be merged with run_and_throw_if_fail

    def run_command(self,
                    args,
                    cwd=None,
                    input=None,
                    error_handler=None,
                    return_exit_code=False,
                    return_stderr=True):
        if hasattr(input, 'read'): # Check if the input is a file.
            stdin = input
            string_to_communicate = None
        else:
            stdin = subprocess.PIPE if input else None
            string_to_communicate = input
        if return_stderr:
            stderr = subprocess.STDOUT
        else:
            stderr = None

        process = subprocess.Popen(args,
                                   stdin=stdin,
                                   stdout=subprocess.PIPE,
                                   stderr=stderr,
                                   cwd=cwd)
        output = process.communicate(string_to_communicate)[0]
        exit_code = process.wait()
        if exit_code:
            script_error = ScriptError(script_args=args,
                                       exit_code=exit_code,
                                       output=output,
                                       cwd=cwd)
            (error_handler or self.default_error_handler)(script_error)
        if return_exit_code:
            return exit_code
        return output
