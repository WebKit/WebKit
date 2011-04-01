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

"""WebKit Win implementation of the Port interface."""

import logging

from webkitpy.layout_tests.port.webkit import WebKitPort

_log = logging.getLogger("webkitpy.layout_tests.port.win")


class WinPort(WebKitPort):
    """WebKit Win implementation of the Port class."""

    def __init__(self, port_name=None, **kwargs):
        port_name = port_name or 'win'
        WebKitPort.__init__(self, port_name=port_name, **kwargs)
        self._version = 'win7'
        self._operating_system = 'win'

    def baseline_search_path(self):
        # Based on code from old-run-webkit-tests expectedDirectoryForTest()
        port_names = ["win", "mac-snowleopard", "mac"]
        return map(self._webkit_baseline_path, port_names)

    def _tests_for_other_platforms(self):
        # FIXME: This list could be dynamic based on platform name and
        # pushed into base.Port.
        # This really need to be automated.
        return [
            "platform/chromium",
            "platform/gtk",
            "platform/qt",
            "platform/mac",
        ]

    def _path_to_apache_config_file(self):
        return self._filesystem.join(self.layout_tests_dir(), 'http', 'conf',
                                     'cygwin-httpd.conf')

    def _shut_down_http_server(self, server_pid):
        """Shut down the httpd web server. Blocks until it's fully
        shut down.

        Args:
            server_pid: The process ID of the running server.
        """
        # Looks like we ignore server_pid.
        # Copy/pasted from chromium-win.
        self._executive.kill_all("httpd.exe")
