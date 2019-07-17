# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2014 Igalia S.L.
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
import re
import time

from webkitpy.port.server_process import ServerProcess
from webkitpy.port.driver import Driver

_log = logging.getLogger(__name__)


class XvfbDriver(Driver):
    @staticmethod
    def check_driver(port):
        xvfb_findcmd = ['which', 'Xvfb']
        if port._should_use_jhbuild():
                xvfb_findcmd = port._jhbuild_wrapper + xvfb_findcmd
        xvfb_found = port.host.executive.run_command(xvfb_findcmd, return_exit_code=True) is 0
        if not xvfb_found:
            _log.error("No Xvfb found. Cannot run layout tests.")
        return xvfb_found

    def _xvfb_pipe(self):
        return os.pipe()

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
                display_id = os.read(read_fd, 256).strip('\n')
                fd_set = []

        return int(display_id)

    def _xvfb_close_pipe(self, pipe_fds):
        os.close(pipe_fds[0])
        os.close(pipe_fds[1])

    def _xvfb_run(self, environment):
        read_fd, write_fd = self._xvfb_pipe()
        run_xvfb = ["Xvfb", "-displayfd", str(write_fd), "-screen",  "0", "1024x768x%s" % self._xvfb_screen_depth(), "-nolisten", "tcp"]
        if self._port._should_use_jhbuild():
            run_xvfb = self._port._jhbuild_wrapper + run_xvfb
        with open(os.devnull, 'w') as devnull:
            self._xvfb_process = self._port.host.executive.popen(run_xvfb, stderr=devnull, env=environment)
            display_id = self._xvfb_read_display_id(read_fd)

        self._xvfb_close_pipe((read_fd, write_fd))

        return display_id

    def _xvfb_screen_depth(self):
        return os.environ.get('XVFB_SCREEN_DEPTH', '24')

    def _setup_environ_for_test(self):
        port_server_environment = self._port.setup_environ_for_server(self._server_name)
        driver_environment = super(XvfbDriver, self)._setup_environ_for_test()
        display_id = self._xvfb_run(port_server_environment)

        # We must do this here because the DISPLAY number depends on _worker_number
        driver_environment['DISPLAY'] = ":%d" % display_id
        driver_environment['UNDER_XVFB'] = 'yes'
        driver_environment['GDK_BACKEND'] = 'x11'
        driver_environment['LOCAL_RESOURCE_ROOT'] = self._port.layout_tests_dir()
        return driver_environment

    def stop(self):
        super(XvfbDriver, self).stop()
        if getattr(self, '_xvfb_process', None):
            self._port.host.executive.kill_process(self._xvfb_process.pid)
            self._xvfb_process = None
