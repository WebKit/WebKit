# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2014-2021 Igalia S.L.
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

import logging
import os
import sys
import re
import time

if sys.version_info[0] < 3:
    try:
        import subprocess32 as subprocess
    except ImportError:
        import subprocess
else:
    import subprocess

from webkitcorepy import string_utils
from webkitpy.port.server_process import ServerProcess
from webkitpy.port.driver import Driver

_log = logging.getLogger(__name__)


class XvfbDriver(Driver):

    def __init__(self, *args, **kwargs):
        Driver.__init__(self, *args, **kwargs)
        self._xvfb_process = None
        self._current_retry_start_xvfb = 0
        self._print_screen_size_process_for_testing = None  # required for unit tests

    @staticmethod
    def check_driver(port):
        xvfb_findcmd = ['which', 'Xvfb']
        if port._should_use_jhbuild():
                xvfb_findcmd = port._jhbuild_wrapper + xvfb_findcmd
        xvfb_found = port.host.executive.run_command(xvfb_findcmd, return_exit_code=True) == 0
        if not xvfb_found:
            _log.error('No Xvfb found. Cannot run layout tests.')
        return xvfb_found

    def _xvfb_pipe(self):
        read_fd, write_fd = os.pipe()

        # By default, python3 creates file descriptors as non-inheritable
        if sys.version_info.major == 3:
            os.set_inheritable(write_fd, True)

        return (read_fd, write_fd)

    def _xvfb_read_display_id(self, read_fd):
        import errno
        import select

        fd_set = [read_fd]
        while fd_set:
            try:
                fd_list = select.select(fd_set, [], [])[0]
            except select.error as e:
                if e.args[0] == errno.EINTR:
                    continue
                raise

            if read_fd in fd_list:
                # We only expect a number, so first read should be enough.
                display_id = os.read(read_fd, 256).strip(string_utils.encode('\n'))
                fd_set = []

        return int(display_id)

    def _xvfb_close_pipe(self, pipe_fds):
        os.close(pipe_fds[0])
        os.close(pipe_fds[1])

    def _xvfb_run(self, environment):
        read_fd, write_fd = self._xvfb_pipe()
        run_xvfb = ['Xvfb', '-displayfd', str(write_fd), '-nolisten', 'tcp',
                    '+extension', 'GLX', '-ac', '-screen', '0',
                    '%sx%s' % (self._xvfb_screen_size(), self._xvfb_screen_depth())]
        if self._port._should_use_jhbuild():
            run_xvfb = self._port._jhbuild_wrapper + run_xvfb
        with open(os.devnull, 'w') as devnull:
            self._xvfb_process = self._port.host.executive.popen(run_xvfb, stderr=devnull, env=environment, close_fds=False)
        display_id = self._xvfb_read_display_id(read_fd)
        self._xvfb_close_pipe((read_fd, write_fd))
        return display_id

    def _xvfb_screen_depth(self):
        return os.environ.get('XVFB_SCREEN_DEPTH', '24')

    def _xvfb_screen_size(self):
        return os.environ.get('XVFB_SCREEN_SIZE', '1024x768')

    def _xvfb_stop(self):
        if self._xvfb_process:
            self._port.host.executive.kill_process(self._xvfb_process.pid)
            self._xvfb_process = None

    def _xvfb_check_if_ready(self, display_id):
        environment_print_screen_size_process = super(XvfbDriver, self)._setup_environ_for_test()
        environment_print_screen_size_process['DISPLAY'] = ':%d' % display_id
        environment_print_screen_size_process['GDK_BACKEND'] = 'x11'
        waited_seconds_for_xvfb_ready = 0
        xvfb_server_replying_as_expected = False
        while True:
            timeout_expired = False
            query_failed = False
            print_screen_size_process = self._print_screen_size_process_for_testing if self._print_screen_size_process_for_testing else \
                subprocess.Popen([self._port.path_from_webkit_base('Tools', 'gtk', 'print-screen-size')],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=environment_print_screen_size_process)
            # Python2 standard subprocess don't allows setting a timeout
            if not hasattr(subprocess, 'TimeoutExpired'):
                stdout, stderr = print_screen_size_process.communicate()
            else:
                try:
                    stdout, stderr = print_screen_size_process.communicate(timeout=2)
                except subprocess.TimeoutExpired:
                    _log.debug('Timeout expired trying to query the Xvfb display server.')
                    timeout_expired = True
                    print_screen_size_process.kill()
                    stdout, stderr = print_screen_size_process.communicate()
                    waited_seconds_for_xvfb_ready += 2

            if not timeout_expired:
                if print_screen_size_process.returncode == 0:
                    queried_screen_size = stdout.decode('UTF-8').strip()
                    if queried_screen_size == self._xvfb_screen_size():
                        xvfb_server_replying_as_expected = True
                        _log.debug('The Xvfb display server ":%d" is ready and replying as expected.' % display_id)
                        break
                    else:
                        _log.warning('The queried Xvfb screen size "%s" does not match the expectation "%s".' % (queried_screen_size, self._xvfb_screen_size()))
                else:
                    _log.warning('The print-screen-size tool returned non-zero status. stdout is "%s" and stderr is "%s"' % (stdout, stderr))
                    query_failed = True
            if timeout_expired or query_failed:
                if self._xvfb_process.poll():
                    _log.error('The Xvfb display server has exited unexpectedly with a return code of %s' % (self._xvfb_process.poll()))
                    break
            if waited_seconds_for_xvfb_ready > 5:
                _log.error('Timeout reached meanwhile waiting for the Xvfb display server to be ready')
                break
            _log.debug('Waiting for Xvfb display server to be ready.')
            if not self._print_screen_size_process_for_testing:
                time.sleep(1)  # only wait when not running unit tests
            waited_seconds_for_xvfb_ready += 1
        return xvfb_server_replying_as_expected

    def _setup_environ_for_test(self):
        port_server_environment = self._port.setup_environ_for_server(self._server_name)
        driver_environment = super(XvfbDriver, self)._setup_environ_for_test()
        display_id = self._xvfb_run(port_server_environment)

        # We must do this here because the DISPLAY number depends on _worker_number
        driver_environment['DISPLAY'] = ":%d" % display_id
        driver_environment['UNDER_XVFB'] = 'yes'
        driver_environment['GDK_BACKEND'] = 'x11'
        driver_environment['LOCAL_RESOURCE_ROOT'] = self._port.layout_tests_dir()

        # Ensure that Xvfb is ready and replying and expected before continuing, give it 3 tries.
        if not self._xvfb_check_if_ready(display_id):
            self._current_retry_start_xvfb += 1
            if self._current_retry_start_xvfb > 3:
                _log.error('Failed to start Xvfb display server ... giving up after 3 retries.')
                raise RuntimeError('Unable to start Xvfb display server')
            _log.error('Failed to start Xvfb display server ... retrying [ %s of 3 ].' % self._current_retry_start_xvfb)
            return self._setup_environ_for_test()

        return driver_environment

    def has_crashed(self):
        if self._xvfb_process and self._xvfb_process.poll():
            self._crashed_process_name = 'Xvfb'
            self._crashed_pid = self._xvfb_process.pid
            return True
        return super(XvfbDriver, self).has_crashed()

    def stop(self):
        super(XvfbDriver, self).stop()
        self._xvfb_stop()
