# Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

import errno
import logging
import signal
import sys
import time

from webkitcorepy import string_utils

# Note that although win32 python does provide an implementation of
# the win32 select API, it only works on sockets, and not on the named pipes
# used by subprocess, so we have to use the native APIs directly.
if sys.platform.startswith('win'):
    import msvcrt
    import pywintypes
    import win32api
    import win32file
    import win32pipe
    import winerror
else:
    import fcntl
    import os
    import select


_log = logging.getLogger(__name__)


class ServerProcess(object):
    """This class provides a wrapper around a subprocess that
    implements a simple request/response usage model. The primary benefit
    is that reading responses takes a deadline, so that we don't ever block
    indefinitely. The class also handles transparently restarting processes
    as necessary to keep issuing commands."""

    def __init__(self, port_obj, name, cmd, env=None, universal_newlines=False, treat_no_data_as_crash=False, target_host=None, crash_message=None, allow_emulation=True):
        self._port = port_obj
        self._name = name  # Should be the command name (e.g. DumpRenderTree, ImageDiff)

        platform = self._port.host.platform
        if allow_emulation and platform.is_mac() and platform.architecture() != self._port.architecture():
            self._cmd = ['/usr/bin/arch', '-{}'.format(self._port.architecture())] + cmd
        else:
            self._cmd = cmd
        self._env = env
        self._crash_message = crash_message or 'This test marked as a crash'

        # Set if the process outputs non-standard newlines like '\r\n' or '\r'.
        # Don't set if there will be binary data or the data must be ASCII encoded.
        self._universal_newlines = universal_newlines
        self._treat_no_data_as_crash = treat_no_data_as_crash
        self._target_host = target_host or port_obj.host
        self._pid = None
        self._system_pid = None
        self._child_processes = {}
        self._reset()

        # See comment in imports for why we need the win32 APIs and can't just use select.
        # FIXME: there should be a way to get win32 vs. cygwin from platforminfo.
        self._use_win32_apis = sys.platform.startswith('win')

    def child_processes(self):
        return self._child_processes

    def set_child_processes(self, child_processes):
        self._child_processes = child_processes

    def pid(self):
        return self._pid

    def system_pid(self):
        return self._system_pid

    def _reset(self):
        if getattr(self, '_proc', None):
            if self._proc.stdin:
                self._proc.stdin.close()
                self._proc.stdin = None
            if self._proc.stdout:
                self._proc.stdout.close()
                self._proc.stdout = None
            if self._proc.stderr:
                self._proc.stderr.close()
                self._proc.stderr = None

        self._proc = None
        self._output = b''
        self._error = b''
        self._crashed = False
        self.timed_out = False

    def process_name(self):
        return self._name

    @staticmethod
    def _set_file_nonblocking(file):
        flags = fcntl.fcntl(file.fileno(), fcntl.F_GETFL)
        fcntl.fcntl(file.fileno(), fcntl.F_SETFL, flags | os.O_NONBLOCK)

    def _start(self):
        if self._proc:
            raise ValueError("%s already running" % self._name)
        self._reset()
        self._proc = self._target_host.executive.popen(self._cmd, stdin=self._target_host.executive.PIPE,
            stdout=self._target_host.executive.PIPE,
            stderr=self._target_host.executive.PIPE,
            close_fds=self._should_close_fds(),
            env=self._env,
            universal_newlines=self._universal_newlines)
        self._pid = self._proc.pid
        self._system_pid = int(self._port._filesystem.read_text_file('/proc/%d/winpid' % self._pid)) if self._port.host.platform.is_cygwin() else self._pid
        self._child_processes = {}
        if not self._use_win32_apis:
            self._set_file_nonblocking(self._proc.stdout)
            self._set_file_nonblocking(self._proc.stderr)

    def _should_close_fds(self):
        # We need to pass close_fds=True to work around Python bug #2320
        # (otherwise we can hang when we kill DumpRenderTree when we are running
        # multiple threads). See http://bugs.python.org/issue2320 .
        # In Python 2.7.10, close_fds is also supported on Windows.
        # However, "you cannot set close_fds to true and also redirect the standard
        # handles by setting stdin, stdout or stderr.".
        platform = self._port.host.platform
        if platform.is_win() and not platform.is_cygwin():
            return False
        else:
            return True

    def _handle_possible_interrupt(self):
        """This routine checks to see if the process crashed or exited
        because of a keyboard interrupt and raises KeyboardInterrupt
        accordingly."""
        # FIXME: Linux and Mac set the returncode to -signal.SIGINT if a
        # subprocess is killed with a ctrl^C.  Previous comments in this
        # routine said that supposedly Windows returns 0xc000001d, but that's not what
        # -1073741510 evaluates to. Figure out what the right value is
        # for win32 and cygwin here ...
        if self._proc.returncode in (-1073741510, -signal.SIGINT):
            raise KeyboardInterrupt

    def poll(self):
        """Check to see if the underlying process is running; returns None
        if it still is (wrapper around subprocess.poll)."""
        if self._proc:
            return self._proc.poll()
        return None

    def write(self, bytes, ignore_crash=False):
        """Write a request to the subprocess. The subprocess is (re-)start()'ed
        if is not already running."""
        if not self._proc:
            self._start()
        try:
            self._proc.stdin.write(string_utils.encode(bytes))
            self._proc.stdin.flush()
        except (IOError, ValueError):
            self.stop(0.0)
            # stop() calls _reset(), so we have to set crashed to True after calling stop()
            # unless we already know that this is a timeout.
            if not ignore_crash:
                _log.debug('{} because of a broken pipe when writing to stdin of the server process.'.format(self._crash_message))
                self._crashed = True

    def _pop_stdout_line_if_ready(self):
        index_after_newline = self._output.find(b'\n') + 1
        if index_after_newline > 0:
            return self._pop_output_bytes(index_after_newline)
        return None

    def _pop_stderr_line_if_ready(self):
        index_after_newline = self._error.find(b'\n') + 1
        if index_after_newline > 0:
            return self._pop_error_bytes(index_after_newline)
        return None

    def pop_all_buffered_stdout(self):
        return self._pop_output_bytes(len(self._output))

    def pop_all_buffered_stderr(self):
        return self._pop_error_bytes(len(self._error))

    def read_stdout_line(self, deadline):
        return self._read(deadline, self._pop_stdout_line_if_ready)

    def has_available_stdout(self):
        if not self.has_crashed() and self._use_win32_apis:
            self._wait_for_data_and_update_buffers_using_win32_apis(0)
        elif not self.has_crashed():
            self._wait_for_data_and_update_buffers_using_select(0)

        return bool(self._output)

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

    def _log(self, message):
        # This is a bit of a hack, but we first log a blank line to avoid
        # messing up the master process's output.
        _log.info('')
        _log.info(message)

    def _handle_timeout(self):
        self.timed_out = True
        if self._port.get_option("sample_on_timeout"):
            self._port.sample_process(self._name, self._proc.pid, self._target_host)

    def _split_string_after_index(self, string, index):
        return string[:index], string[index:]

    def _pop_output_bytes(self, bytes_count):
        output, self._output = self._split_string_after_index(self._output, bytes_count)
        return output

    def _pop_error_bytes(self, bytes_count):
        output, self._error = self._split_string_after_index(self._error, bytes_count)
        return output

    def _wait_for_data_and_update_buffers_using_select(self, deadline, stopping=False):
        if not self._proc or self._proc.stdout.closed or self._proc.stderr.closed:
            # If the process crashed and is using FIFOs, like Chromium Android, the
            # stdout and stderr pipes will be closed.
            return

        out_fd = self._proc.stdout.fileno()
        err_fd = self._proc.stderr.fileno()
        select_fds = (out_fd, err_fd)
        try:
            read_fds, _, _ = select.select(select_fds, [], select_fds, max(deadline - time.time(), 0))
        except select.error as e:
            # We can ignore EINVAL since it's likely the process just crashed and we'll
            # figure that out the next time through the loop in _read().
            # We also ignore EINTR as we can resume reading the next time
            # through the loop in _read().
            if e.args[0] in [errno.EINVAL, errno.EINTR]:
                return
            raise

        try:
            # Note that we may get no data during read() even though
            # select says we got something; see the select() man page
            # on linux. I don't know if this happens on Mac OS and
            # other Unixen as well, but we don't bother special-casing
            # Linux because it's relatively harmless either way.
            if out_fd in read_fds:
                data = self._proc.stdout.read()
                if not data and not stopping and (self._treat_no_data_as_crash or self._proc.poll()):
                    _log.debug('{} because of no data while reading stdout for the server process.'.format(self._crash_message))
                    self._crashed = True
                self._output += data

            if err_fd in read_fds:
                data = self._proc.stderr.read()
                if not data and not stopping and (self._treat_no_data_as_crash or self._proc.poll()):
                    _log.debug('{} because of no data while reading stdout for the server process.'.format(self._crash_message))
                    self._crashed = True
                self._error += data
        except IOError:
            # We can ignore the IOErrors because we will detect if the subporcess crashed
            # the next time through the loop in _read()
            pass

    def _wait_for_data_and_update_buffers_using_win32_apis(self, deadline):
        if not self._proc:
            return

        # See http://code.activestate.com/recipes/440554-module-to-allow-asynchronous-subprocess-use-on-win/
        # and http://docs.activestate.com/activepython/2.6/pywin32/modules.html
        # for documentation on all of these win32-specific modules.
        out_fh = msvcrt.get_osfhandle(self._proc.stdout.fileno())
        err_fh = msvcrt.get_osfhandle(self._proc.stderr.fileno())
        checking = True
        while checking:
            output = self._non_blocking_read_win32(out_fh)
            error = self._non_blocking_read_win32(err_fh)
            if output or error:
                if output:
                    self._output += output
                if error:
                    self._error += error
                return
            if self._proc.poll() is not None:
                return
            time.sleep(0.01)
            checking = time.time() < deadline

    def _non_blocking_read_win32(self, handle):
        try:
            _, avail, _ = win32pipe.PeekNamedPipe(handle, 0)
            if avail > 0:
                _, buf = win32file.ReadFile(handle, avail, None)
                return buf
        except pywintypes.error as e:
            if e.winerror not in (winerror.ERROR_INVALID_FUNCTION, winerror.ERROR_BROKEN_PIPE, errno.ESHUTDOWN):
                raise
        return None

    def has_crashed(self):
        if not self._crashed and self.poll():
            _log.debug('{} because of failure to poll the server process (return code was {}).'.format(self._crash_message, self._proc.returncode))
            self._crashed = True
            self._handle_possible_interrupt()
        return self._crashed

    # This read function is a bit oddly-designed, as it polls both stdout and stderr, yet
    # only reads/returns from one of them (buffering both in local self._output/self._error).
    # It might be cleaner to pass in the file descriptor to poll instead.
    def _read(self, deadline, fetch_bytes_from_buffers_callback):
        while True:
            # Polling does not need to occur before bytes are fetched from the buffer.
            if self._crashed:
                return None

            if time.time() > deadline:
                self._handle_timeout()
                return None

            bytes = fetch_bytes_from_buffers_callback()
            if bytes is not None:
                return bytes

            if self.has_crashed():
                return None

            if self._use_win32_apis:
                self._wait_for_data_and_update_buffers_using_win32_apis(deadline)
            else:
                self._wait_for_data_and_update_buffers_using_select(deadline)

    def start(self):
        if not self._proc:
            self._start()

    def stop(self, timeout_secs=3.0):
        if not self._proc:
            return (None, None)

        # Only bother to check for leaks or stderr if the process is still running.
        if self.poll() is None:
            self._port.check_for_leaks(self.process_name(), self.pid())
            for child_process_name in self._child_processes.keys():
                for child_process_id in self._child_processes[child_process_name]:
                    self._port.check_for_leaks(child_process_name, child_process_id)

        if self._proc.stdin:
            self._proc.stdin.close()
            self._proc.stdin = None

        return self._wait_for_stop(timeout_secs)

    def _wait_for_stop(self, timeout_secs=3.0):
        now = time.time()
        killed = False
        if timeout_secs:
            deadline = now + timeout_secs
            while self._proc and self._proc.poll() is None and time.time() < deadline:
                time.sleep(0.01)
            if self._proc and self._proc.poll() is None:
                _log.warning('stopping %s(pid %d) timed out, killing it' % (self._name, self._proc.pid))

        if self._proc and self._proc.poll() is None:
            self._kill()
            killed = True
            _log.debug('killed pid %d' % self._proc.pid)

        # read any remaining data on the pipes and return it.
        if self._proc and not killed:
            if self._use_win32_apis:
                self._wait_for_data_and_update_buffers_using_win32_apis(now)
            else:
                self._wait_for_data_and_update_buffers_using_select(now, stopping=True)
        out, err = self._output, self._error
        self._reset()
        return (out, err)

    def kill(self):
        self.stop(0.0)

    def _kill(self):
        self._target_host.executive.kill_process(self._proc.pid)
        if self._proc.poll() is None:
            self._proc.wait()

    def replace_outputs(self, stdout, stderr):
        assert self._proc
        if stdout:
            self._proc.stdout.close()
            self._proc.stdout = stdout
        if stderr:
            self._proc.stderr.close()
            self._proc.stderr = stderr
