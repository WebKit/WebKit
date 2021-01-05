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

import logging
import os
import re
import time

from webkitpy.port.server_process import ServerProcess
from webkitpy.port.driver import Driver
from webkitpy.common.system.file_lock import FileLock

_log = logging.getLogger(__name__)


class HaikuDriver(Driver):
    @staticmethod
    def check_driver(port):
        return 1

    def __init__(self, *args, **kwargs):
        Driver.__init__(self, *args, **kwargs)
        self._guard_lock = None
        self._startup_delay_secs = 1.0

    def _next_free_display(self):
        return 0

    def _xvfb_screen_depth(self):
        return 24

    def _start(self, pixel_tests, per_test_args):
        self.stop()

        # Use even displays for pixel tests and odd ones otherwise. When pixel tests are disabled,
        # DriverProxy creates two drivers, one for normal and the other for ref tests. Both have
        # the same worker number, so this prevents them from using the same Xvfb instance.
        display_id = self._next_free_display()

        server_name = "haiku"
        environment = self._port.setup_environ_for_server(server_name)
        self._driver_tempdir = self._port.host.filesystem.mkdtemp(prefix='%s-' % self._port.driver_name())
        environment['DUMPRENDERTREE_TEMP'] = str(self._driver_tempdir)
        environment['LOCAL_RESOURCE_ROOT'] = self._port.layout_tests_dir()

        # Currently on WebKit2, there is no API for setting the application
        # cache directory. Each worker should have it's own and it should be
        # cleaned afterwards, so we set it to inside the temporary folder by
        # prepending XDG_CACHE_HOME with DUMPRENDERTREE_TEMP.
        environment['XDG_CACHE_HOME'] = self._port.host.filesystem.join(str(self._driver_tempdir), 'appcache')

        self._crashed_process_name = None
        self._crashed_pid = None
        self._server_process = self._port._server_process_constructor(self._port, server_name, self.cmd_line(pixel_tests, per_test_args), environment)
        self._server_process.start()

    def stop(self):
        super(HaikuDriver, self).stop()
        if self._guard_lock:
            self._guard_lock.release_lock()
            self._guard_lock = None
