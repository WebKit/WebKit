#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
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
#     * Neither the Google name nor the names of its
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

"""Package that implements the ServerProcess wrapper class"""

import logging
import os
import select
import signal
import subprocess
import sys
import time
if sys.platform != 'win32':
    import fcntl

from webkitpy.common.system.executive import Executive, ScriptError


_log = logging.getLogger(__name__)


class ServerProcess:
    """This class provides a wrapper around a subprocess that
    implements a simple request/response usage model. The primary benefit
    is that reading responses takes a deadline, so that we don't ever block
    indefinitely. The class also handles transparently restarting processes
    as necessary to keep issuing commands."""

    def __init__(self, port_obj, name, cmd, env=None, executive=Executive()):
        self._port = port_obj
        self._name = name  # Should be the command name (e.g. DumpRenderTree, ImageDiff)
        self._cmd = cmd
        self._env = env
        self._reset()
        self._executive = executive

    def name(self):
        return self._name

    def pid(self):
        return self._proc.pid

    def _reset(self):
        self._proc = None
        self._output = str()  # bytesarray() once we require Python 2.6
        self._error = str()  # bytesarray() once we require Python 2.6
        self.set_crashed(False)
        self.timed_out = False

    def process_name(self):
        return self._name

    def _start(self):
        if self._proc:
            raise ValueError("%s already running" % self._name)
        self._reset()
        # close_fds is a workaround for http://bugs.python.org/issue2320
        close_fds = sys.platform not in ('win32', 'cygwin')
        self._proc = subprocess.Popen(self._cmd, stdin=subprocess.PIPE,
                                      stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE,
                                      close_fds=close_fds,
                                      env=self._env)
        fd = self._proc.stdout.fileno()
        fl = fcntl.fcntl(fd, fcntl.F_GETFL)
        fcntl.fcntl(fd, fcntl.F_SETFL, fl | os.O_NONBLOCK)
        fd = self._proc.stderr.fileno()
        fl = fcntl.fcntl(fd, fcntl.F_GETFL)
        fcntl.fcntl(fd, fcntl.F_SETFL, fl | os.O_NONBLOCK)

    def handle_interrupt(self):
        """This routine checks to see if the process crashed or exited
        because of a keyboard interrupt and raises KeyboardInterrupt
        accordingly."""
        if self.crashed:
            # This is hex code 0xc000001d, which is used for abrupt
            # termination. This happens if we hit ctrl+c from the prompt
            # and we happen to be waiting on the DumpRenderTree.
            # sdoyon: Not sure for which OS and in what circumstances the
            # above code is valid. What works for me under Linux to detect
            # ctrl+c is for the subprocess returncode to be negative
            # SIGINT. And that agrees with the subprocess documentation.
            if (-1073741510 == self._proc.returncode or
                - signal.SIGINT == self._proc.returncode):
                raise KeyboardInterrupt
            return

    def poll(self):
        """Check to see if the underlying process is running; returns None
        if it still is (wrapper around subprocess.poll)."""
        if self._proc:
            return self._proc.poll()
        return None

    def write(self, bytes):
        """Write a request to the subprocess. The subprocess is (re-)start()'ed
        if is not already running."""
        if not self._proc:
            self._start()
        try:
            self._proc.stdin.write(bytes)
        except IOError, e:
            self.stop()
            # stop() calls _reset(), so we have to set crashed to True after calling stop().
            self.set_crashed(True)

    def _pop_stdout_line_if_ready(self):
        index_after_newline = self._output.find('\n') + 1
        if index_after_newline > 0:
            return self._pop_output_bytes(index_after_newline)
        return None

    def _pop_stderr_line_if_ready(self):
        index_after_newline = self._error.find('\n') + 1
        if index_after_newline > 0:
            return self._pop_error_bytes(index_after_newline)
        return None

    def pop_all_buffered_stderr(self):
        return self._pop_error_bytes(len(self._error))

    def read_stdout_line(self, deadline):
        return self._read(deadline, self._pop_stdout_line_if_ready)

    def read_stderr_line(self, deadline):
        return self._read(deadline, self._pop_stderr_line_if_ready)

    def read_either_stdout_or_stderr_line(self, deadline):
        def retrieve_bytes_from_buffers():
            stdout_line = self._pop_stdout_line_if_ready()
            if stdout_line:
                return stdout_line, None
            stderr_line = self._pop_stderr_line_if_ready()
            if stderr_line:
                return None, stderr_line
            return None  # Instructs the caller to keep waiting.

        return_value = self._read(deadline, retrieve_bytes_from_buffers)
        # FIXME: This is a bit of a hack around the fact that _read normally only returns one value, but this caller wants it to return two.
        if return_value is None:
            return None, None
        return return_value

    def read_stdout(self, deadline, size):
        if size <= 0:
            raise ValueError('ServerProcess.read() called with a non-positive size: %d ' % size)

        def retrieve_bytes_from_stdout_buffer():
            if len(self._output) >= size:
                return self._pop_output_bytes(size)
            return None

        return self._read(deadline, retrieve_bytes_from_stdout_buffer)

    def _check_for_crash(self, wait_for_crash_reporter=True):
        if self._proc.poll() != None:
            self.set_crashed(True, wait_for_crash_reporter)
            self.handle_interrupt()

    def _log(self, message):
        # This is a bit of a hack, but we first log a blank line to avoid
        # messing up the master process's output.
        _log.info('')
        _log.info(message)

    def _sample(self):
        if sys.platform != "darwin":
            return
        try:
            hang_report = os.path.join(self._port.results_directory(), "%s-%s.sample.txt" % (self._name, self._proc.pid))
            self._executive.run_command([
                "/usr/bin/sample",
                self._proc.pid,
                10,
                10,
                "-file",
                hang_report,
            ])
        except ScriptError, e:
            self._log('Unable to sample process.')

    def _handle_timeout(self):
        self._executive.wait_newest(self._port.is_crash_reporter)
        self._check_for_crash(wait_for_crash_reporter=False)
        if self.crashed:
            return
        self.timed_out = True
        self._sample()

    def _split_string_after_index(self, string, index):
        return string[:index], string[index:]

    def _pop_output_bytes(self, bytes_count):
        output, self._output = self._split_string_after_index(self._output, bytes_count)
        return output

    def _pop_error_bytes(self, bytes_count):
        output, self._error = self._split_string_after_index(self._error, bytes_count)
        return output

    def _wait_for_data_and_update_buffers(self, deadline):
        out_fd = self._proc.stdout.fileno()
        err_fd = self._proc.stderr.fileno()
        select_fds = (out_fd, err_fd)
        read_fds, _, _ = select.select(select_fds, [], select_fds, deadline - time.time())
        try:
            if out_fd in read_fds:
                self._output += self._proc.stdout.read()
            if err_fd in read_fds:
                self._error += self._proc.stderr.read()
        except IOError, e:
            # FIXME: Why do we ignore all IOErrors here?
            pass

    def _check_for_abort(self, deadline):
        self._check_for_crash()

        if time.time() > deadline:
            self._handle_timeout()

        return self.crashed or self.timed_out

    # This read function is a bit oddly-designed, as it polls both stdout and stderr, yet
    # only reads/returns from one of them (buffering both in local self._output/self._error).
    # It might be cleaner to pass in the file descriptor to poll instead.
    def _read(self, deadline, fetch_bytes_from_buffers_callback):
        while True:
            if self._check_for_abort(deadline):
                return None

            bytes = fetch_bytes_from_buffers_callback()
            if bytes is not None:
                return bytes

            self._wait_for_data_and_update_buffers(deadline)

    def stop(self):
        if not self._proc:
            return

        # Only bother to check for leaks if the process is still running.
        if self.poll() is None:
            self._port.check_for_leaks(self.name(), self.pid())

        pid = self._proc.pid
        self._proc.stdin.close()
        self._proc.stdout.close()
        if self._proc.stderr:
            self._proc.stderr.close()
        if sys.platform not in ('win32', 'cygwin'):
            # Closing stdin/stdout/stderr hangs sometimes on OS X,
            # (see restart(), above), and anyway we don't want to hang
            # the harness if DumpRenderTree is buggy, so we wait a couple
            # seconds to give DumpRenderTree a chance to clean up, but then
            # force-kill the process if necessary.
            KILL_TIMEOUT = 3.0
            timeout = time.time() + KILL_TIMEOUT
            while self._proc.poll() is None and time.time() < timeout:
                time.sleep(0.1)
            if self._proc.poll() is None:
                _log.warning('stopping %s timed out, killing it' % self._name)
                self._executive.kill_process(self._proc.pid)
                _log.warning('killed')
        self._reset()

    def set_crashed(self, crashed, wait_for_crash_reporter=True):
        self.crashed = crashed
        if not self.crashed or not wait_for_crash_reporter:
            return
        self._executive.wait_newest(self._port.is_crash_reporter)
