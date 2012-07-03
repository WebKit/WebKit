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
import signal
import subprocess

from webkitpy.layout_tests.port.server_process import ServerProcess
from webkitpy.layout_tests.port.webkit import WebKitDriver
from webkitpy.common.system.executive import Executive
from webkitpy.common.system.file_lock import FileLock
from webkitpy.common.system.filesystem import FileSystem

_log = logging.getLogger(__name__)


class XvfbDriver(WebKitDriver):
    def __init__(self, *args, **kwargs):
        WebKitDriver.__init__(self, *args, **kwargs)
        self._guard_lock = None

    def _start(self, pixel_tests, per_test_args):

        def next_free_id():
            for i in range(99):
                if not os.path.exists('/tmp/.X%d-lock' % i):
                    self._guard_lock = FileLock(self._port._filesystem.join('/tmp', 'WebKitXvfb.lock.%i' % i), 0)
                    if self._guard_lock.acquire_lock():
                        return i

        self._display_id = next_free_id()
        _log.debug('Start new xvfb with id %d for worker/%d' % (self._display_id, self._worker_number))
        run_xvfb = ["Xvfb", ":%d" % (self._display_id), "-screen",  "0", "800x600x24", "-nolisten", "tcp"]
        with open(os.devnull, 'w') as devnull:
            self._xvfb_process = subprocess.Popen(run_xvfb, stderr=devnull)
        self._driver_tempdir = self._port._filesystem.mkdtemp(prefix='%s-' % self._port.driver_name())
        server_name = self._port.driver_name()
        environment = self._port.setup_environ_for_server(server_name)
        # We must do this here because the DISPLAY number depends on _worker_number
        environment['DISPLAY'] = ":%d" % (self._display_id)
        environment['DYLD_LIBRARY_PATH'] = self._port._build_path()
        environment['DYLD_FRAMEWORK_PATH'] = self._port._build_path()
        # FIXME: We're assuming that WebKitTestRunner checks this DumpRenderTree-named environment variable.
        environment['DUMPRENDERTREE_TEMP'] = str(self._driver_tempdir)
        environment['LOCAL_RESOURCE_ROOT'] = self._port.layout_tests_dir()
        self._crashed_process_name = None
        self._crashed_pid = None
        self._server_process = ServerProcess(self._port, server_name, self.cmd_line(pixel_tests, per_test_args), environment)

    def stop(self):
        WebKitDriver.stop(self)
        if self._guard_lock:
            self._guard_lock.release_lock()
            self._guard_lock = None
        if getattr(self, '_xvfb_process', None):
            try:
                self._xvfb_process.terminate()
                self._xvfb_process.wait()
            except OSError:
                _log.warn("The driver is already terminated.")
            self._xvfb_process = None
