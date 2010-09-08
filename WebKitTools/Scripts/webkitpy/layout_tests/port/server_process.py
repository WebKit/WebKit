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

import fcntl
import logging
import os
import select
import signal
import subprocess
import sys
import time

from webkitpy.common.system.executive import Executive

_log = logging.getLogger("webkitpy.layout_tests.port.server_process")


class ServerProcess:
    """This class provides a wrapper around a subprocess that
    implements a simple request/response usage model. The primary benefit
    is that reading responses takes a timeout, so that we don't ever block
    indefinitely. The class also handles transparently restarting processes
    as necessary to keep issuing commands."""

    def __init__(self, port_obj, name, cmd, env=None, executive=Executive()):
        self._port = port_obj
        self._name = name
        self._cmd = cmd
        self._env = env
        self._reset()
        self._executive = executive

    def _reset(self):
        self._proc = None
        self._output = ''
        self.crashed = False
        self.timed_out = False
        self.error = ''

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
            # poll() is not threadsafe and can throw OSError due to:
            # http://bugs.python.org/issue1731717
            return self._proc.poll()
        return None

    def write(self, input):
        """Write a request to the subprocess. The subprocess is (re-)start()'ed
        if is not already running."""
        if not self._proc:
            self._start()
        self._proc.stdin.write(input)

    def read_line(self, timeout):
        """Read a single line from the subprocess, waiting until the deadline.
        If the deadline passes, the call times out. Note that even if the
        subprocess has crashed or the deadline has passed, if there is output
        pending, it will be returned.

        Args:
            timeout: floating-point number of seconds the call is allowed
                to block for. A zero or negative number will attempt to read
                any existing data, but will not block. There is no way to
                block indefinitely.
        Returns:
            output: data returned, if any. If no data is available and the
                call times out or crashes, an empty string is returned. Note
                that the returned string includes the newline ('\n')."""
        return self._read(timeout, size=0)

    def read(self, timeout, size):
        """Attempts to read size characters from the subprocess, waiting until
        the deadline passes. If the deadline passes, any available data will be
        returned. Note that even if the deadline has passed or if the
        subprocess has crashed, any available data will still be returned.

        Args:
            timeout: floating-point number of seconds the call is allowed
                to block for. A zero or negative number will attempt to read
                any existing data, but will not block. There is no way to
                block indefinitely.
            size: amount of data to read. Must be a postive integer.
        Returns:
            output: data returned, if any. If no data is available, an empty
                string is returned.
        """
        if size <= 0:
            raise ValueError('ServerProcess.read() called with a '
                             'non-positive size: %d ' % size)
        return self._read(timeout, size)

    def _read(self, timeout, size):
        """Internal routine that actually does the read."""
        index = -1
        out_fd = self._proc.stdout.fileno()
        err_fd = self._proc.stderr.fileno()
        select_fds = (out_fd, err_fd)
        deadline = time.time() + timeout
        while not self.timed_out and not self.crashed:
            # poll() is not threadsafe and can throw OSError due to:
            # http://bugs.python.org/issue1731717
            if self._proc.poll() != None:
                self.crashed = True
                self.handle_interrupt()

            now = time.time()
            if now > deadline:
                self.timed_out = True

            # Check to see if we have any output we can return.
            if size and len(self._output) >= size:
                index = size
            elif size == 0:
                index = self._output.find('\n') + 1

            if index or self.crashed or self.timed_out:
                output = self._output[0:index]
                self._output = self._output[index:]
                return output

            # Nope - wait for more data.
            (read_fds, write_fds, err_fds) = select.select(select_fds, [],
                                                           select_fds,
                                                           deadline - now)
            try:
                if out_fd in read_fds:
                    self._output += self._proc.stdout.read()
                if err_fd in read_fds:
                    self.error += self._proc.stderr.read()
            except IOError, e:
                pass

    def stop(self):
        """Stop (shut down) the subprocess), if it is running."""
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
            # poll() is not threadsafe and can throw OSError due to:
            # http://bugs.python.org/issue1731717
            while self._proc.poll() is None and time.time() < timeout:
                time.sleep(0.1)
            # poll() is not threadsafe and can throw OSError due to:
            # http://bugs.python.org/issue1731717
            if self._proc.poll() is None:
                _log.warning('stopping %s timed out, killing it' %
                             self._name)
                self._executive.kill_process(self._proc.pid)
                _log.warning('killed')
        self._reset()
