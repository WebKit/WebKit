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

"""QtWebKit implementation of the Port interface."""

import logging
import os
import signal
import sys

import webkit

from webkitpy.layout_tests.port.webkit import WebKitPort

_log = logging.getLogger("webkitpy.layout_tests.port.qt")


class QtPort(WebKitPort):
    """QtWebKit implementation of the Port class."""

    def __init__(self, **kwargs):
        kwargs.setdefault('port_name', 'qt')
        WebKitPort.__init__(self, **kwargs)

    def baseline_search_path(self):
        port_names = []
        if sys.platform == 'linux2':
            port_names.append("qt-linux")
        elif sys.platform in ('win32', 'cygwin'):
            port_names.append("qt-win")
        elif sys.platform == 'darwin':
            port_names.append("qt-mac")
        port_names.append("qt")
        return map(self._webkit_baseline_path, port_names)

    def _tests_for_other_platforms(self):
        # FIXME: This list could be dynamic based on platform name and
        # pushed into base.Port.
        # This really need to be automated.
        return [
            "platform/chromium",
            "platform/win",
            "platform/gtk",
            "platform/mac",
        ]

    def _path_to_apache_config_file(self):
        # FIXME: This needs to detect the distribution and change config files.
        return os.path.join(self.layout_tests_dir(), 'http', 'conf',
                            'apache2-debian-httpd.conf')

    def _shut_down_http_server(self, server_pid):
        """Shut down the httpd web server. Blocks until it's fully
        shut down.

        Args:
            server_pid: The process ID of the running server.
        """
        # server_pid is not set when "http_server.py stop" is run manually.
        if server_pid is None:
            # FIXME: This isn't ideal, since it could conflict with
            # lighttpd processes not started by http_server.py,
            # but good enough for now.
            self._executive.kill_all('apache2')
        else:
            try:
                os.kill(server_pid, signal.SIGTERM)
                # TODO(mmoss) Maybe throw in a SIGKILL just to be sure?
            except OSError:
                # Sometimes we get a bad PID (e.g. from a stale httpd.pid
                # file), so if kill fails on the given PID, just try to
                # 'killall' web servers.
                self._shut_down_http_server(None)

    def _build_driver(self):
        # The Qt port builds DRT as part of the main build step
        return True

    def _path_to_driver(self):
        return self._build_path('bin/DumpRenderTree')

    def _path_to_image_diff(self):
        return self._build_path('bin/ImageDiff')

    def _path_to_webcore_library(self):
        return self._build_path('lib/libQtWebKit.so')

    def _runtime_feature_list(self):
        return None

    def setup_environ_for_server(self):
        env = webkit.WebKitPort.setup_environ_for_server(self)
        env['QTWEBKIT_PLUGIN_PATH'] = self._build_path('lib/plugins')
        return env
