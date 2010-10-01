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

try:
    # This API exists only in Python 2.6 and higher.  :(
    import multiprocessing
except ImportError:
    multiprocessing = None

import errno
import logging
import os
import platform
import StringIO
import signal
import subprocess
import sys
import time

from webkitpy.common.system.deprecated_logging import tee


_log = logging.getLogger("webkitpy.common.system")


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
                return u"%s\nLast %s characters of output:\n%s" % \
                    (self, output_limit, self.output[-output_limit:])
            return u"%s\n%s" % (self, self.output)
        return unicode(self)

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

    def _should_close_fds(self):
        # We need to pass close_fds=True to work around Python bug #2320
        # (otherwise we can hang when we kill DumpRenderTree when we are running
        # multiple threads). See http://bugs.python.org/issue2320 .
        # Note that close_fds isn't supported on Windows, but this bug only
        # shows up on Mac and Linux.
        return sys.platform not in ('win32', 'cygwin')

    def _run_command_with_teed_output(self, args, teed_output):
        args = map(unicode, args)  # Popen will throw an exception if args are non-strings (like int())
        if sys.platform == 'cygwin':
            # Cygwin's Python's os.execv doesn't support unicode command
            # arguments, and neither does Cygwin's execv itself.
            # FIXME: Using UTF-8 here will confuse Windows-native commands
            # which will expect arguments to be encoded using the current code
            # page.
            args = [arg.encode('utf-8') for arg in args]
        child_process = subprocess.Popen(args,
                                         stdout=subprocess.PIPE,
                                         stderr=subprocess.STDOUT,
                                         close_fds=self._should_close_fds())

        # Use our own custom wait loop because Popen ignores a tee'd
        # stderr/stdout.
        # FIXME: This could be improved not to flatten output to stdout.
        while True:
            output_line = child_process.stdout.readline()
            if output_line == "" and child_process.poll() != None:
                # poll() is not threadsafe and can throw OSError due to:
                # http://bugs.python.org/issue1731717
                return child_process.poll()
            # We assume that the child process wrote to us in utf-8,
            # so no re-encoding is necessary before writing here.
            teed_output.write(output_line)

    # FIXME: Remove this deprecated method and move callers to run_command.
    # FIXME: This method is a hack to allow running command which both
    # capture their output and print out to stdin.  Useful for things
    # like "build-webkit" where we want to display to the user that we're building
    # but still have the output to stuff into a log file.
    def run_and_throw_if_fail(self, args, quiet=False, decode_output=True):
        # Cache the child's output locally so it can be used for error reports.
        child_out_file = StringIO.StringIO()
        tee_stdout = sys.stdout
        if quiet:
            dev_null = open(os.devnull, "w")  # FIXME: Does this need an encoding?
            tee_stdout = dev_null
        child_stdout = tee(child_out_file, tee_stdout)
        exit_code = self._run_command_with_teed_output(args, child_stdout)
        if quiet:
            dev_null.close()

        child_output = child_out_file.getvalue()
        child_out_file.close()

        # We assume the child process output utf-8
        if decode_output:
            child_output = child_output.decode("utf-8")

        if exit_code:
            raise ScriptError(script_args=args,
                              exit_code=exit_code,
                              output=child_output)
        return child_output

    def cpu_count(self):
        if multiprocessing:
            return multiprocessing.cpu_count()
        # Darn.  We don't have the multiprocessing package.
        system_name = platform.system()
        if system_name == "Darwin":
            return int(self.run_command(["sysctl", "-n", "hw.ncpu"]))
        elif system_name == "Windows":
            return int(os.environ.get('NUMBER_OF_PROCESSORS', 1))
        elif system_name == "Linux":
            num_cores = os.sysconf("SC_NPROCESSORS_ONLN")
            if isinstance(num_cores, int) and num_cores > 0:
                return num_cores
        # This quantity is a lie but probably a reasonable guess for modern
        # machines.
        return 2

    def kill_process(self, pid):
        """Attempts to kill the given pid.
        Will fail silently if pid does not exist or insufficient permisssions."""
        if sys.platform == "win32":
            # We only use taskkill.exe on windows (not cygwin) because subprocess.pid
            # is a CYGWIN pid and taskkill.exe expects a windows pid.
            # Thankfully os.kill on CYGWIN handles either pid type.
            command = ["taskkill.exe", "/f", "/pid", pid]
            # taskkill will exit 128 if the process is not found.  We should log.
            self.run_command(command, error_handler=self.ignore_error)
            return

        # According to http://docs.python.org/library/os.html
        # os.kill isn't available on Windows. python 2.5.5 os.kill appears
        # to work in cygwin, however it occasionally raises EAGAIN.
        retries_left = 10 if sys.platform == "cygwin" else 1
        while retries_left > 0:
            try:
                retries_left -= 1
                os.kill(pid, signal.SIGKILL)
            except OSError, e:
                if e.errno == errno.EAGAIN:
                    if retries_left <= 0:
                        _log.warn("Failed to kill pid %s.  Too many EAGAIN errors." % pid)
                    continue
                if e.errno == errno.ESRCH:  # The process does not exist.
                    _log.warn("Called kill_process with a non-existant pid %s" % pid)
                    return
                raise

    def _windows_image_name(self, process_name):
        name, extension = os.path.splitext(process_name)
        if not extension:
            # taskkill expects processes to end in .exe
            # If necessary we could add a flag to disable appending .exe.
            process_name = "%s.exe" % name
        return process_name

    def kill_all(self, process_name):
        """Attempts to kill processes matching process_name.
        Will fail silently if no process are found."""
        if sys.platform in ("win32", "cygwin"):
            image_name = self._windows_image_name(process_name)
            command = ["taskkill.exe", "/f", "/im", image_name]
            # taskkill will exit 128 if the process is not found.  We should log.
            self.run_command(command, error_handler=self.ignore_error)
            return

        # FIXME: This is inconsistent that kill_all uses TERM and kill_process
        # uses KILL.  Windows is always using /f (which seems like -KILL).
        # We should pick one mode, or add support for switching between them.
        # Note: Mac OS X 10.6 requires -SIGNALNAME before -u USER
        command = ["killall", "-TERM", "-u", os.getenv("USER"), process_name]
        # killall returns 1 if no process can be found and 2 on command error.
        # FIXME: We should pass a custom error_handler to allow only exit_code 1.
        # We should log in exit_code == 1
        self.run_command(command, error_handler=self.ignore_error)

    # Error handlers do not need to be static methods once all callers are
    # updated to use an Executive object.

    @staticmethod
    def default_error_handler(error):
        raise error

    @staticmethod
    def ignore_error(error):
        pass

    def _compute_stdin(self, input):
        """Returns (stdin, string_to_communicate)"""
        # FIXME: We should be returning /dev/null for stdin
        # or closing stdin after process creation to prevent
        # child processes from getting input from the user.
        if not input:
            return (None, None)
        if hasattr(input, "read"):  # Check if the input is a file.
            return (input, None)  # Assume the file is in the right encoding.

        # Popen in Python 2.5 and before does not automatically encode unicode objects.
        # http://bugs.python.org/issue5290
        # See https://bugs.webkit.org/show_bug.cgi?id=37528
        # for an example of a regresion caused by passing a unicode string directly.
        # FIXME: We may need to encode differently on different platforms.
        if isinstance(input, unicode):
            input = input.encode("utf-8")
        return (subprocess.PIPE, input)

    def _command_for_printing(self, args):
        """Returns a print-ready string representing command args.
        The string should be copy/paste ready for execution in a shell."""
        escaped_args = []
        for arg in args:
            if isinstance(arg, unicode):
                # Escape any non-ascii characters for easy copy/paste
                arg = arg.encode("unicode_escape")
            # FIXME: Do we need to fix quotes here?
            escaped_args.append(arg)
        return " ".join(escaped_args)

    # FIXME: run_and_throw_if_fail should be merged into this method.
    def run_command(self,
                    args,
                    cwd=None,
                    input=None,
                    error_handler=None,
                    return_exit_code=False,
                    return_stderr=True,
                    decode_output=True):
        """Popen wrapper for convenience and to work around python bugs."""
        assert(isinstance(args, list) or isinstance(args, tuple))
        start_time = time.time()
        args = map(unicode, args)  # Popen will throw an exception if args are non-strings (like int())
        if sys.platform == 'cygwin':
            # Cygwin's Python's os.execv doesn't support unicode command
            # arguments, and neither does Cygwin's execv itself.
            # FIXME: Using UTF-8 here will confuse Windows-native commands
            # which will expect arguments to be encoded using the current code
            # page.
            args = [arg.encode('utf-8') for arg in args]
        stdin, string_to_communicate = self._compute_stdin(input)
        stderr = subprocess.STDOUT if return_stderr else None

        process = subprocess.Popen(args,
                                   stdin=stdin,
                                   stdout=subprocess.PIPE,
                                   stderr=stderr,
                                   cwd=cwd,
                                   close_fds=self._should_close_fds())
        output = process.communicate(string_to_communicate)[0]
        # run_command automatically decodes to unicode() unless explicitly told not to.
        if decode_output:
            output = output.decode("utf-8")
        # wait() is not threadsafe and can throw OSError due to:
        # http://bugs.python.org/issue1731717
        exit_code = process.wait()

        _log.debug('"%s" took %.2fs' % (self._command_for_printing(args), time.time() - start_time))

        if return_exit_code:
            return exit_code

        if exit_code:
            script_error = ScriptError(script_args=args,
                                       exit_code=exit_code,
                                       output=output,
                                       cwd=cwd)
            (error_handler or self.default_error_handler)(script_error)
        return output
