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

"""Chromium Mac implementation of the Port interface."""

import logging
import os
import platform
import signal

import chromium

from webkitpy.common.system.executive import Executive

_log = logging.getLogger("webkitpy.layout_tests.port.chromium_mac")


class ChromiumMacPort(chromium.ChromiumPort):
    """Chromium Mac implementation of the Port class."""

    def __init__(self, **kwargs):
        kwargs.setdefault('port_name', 'chromium-mac')
        chromium.ChromiumPort.__init__(self, **kwargs)

    def baseline_search_path(self):
        port_names = ["chromium-mac", "chromium", "mac" + self.version(), "mac"]
        return map(self._webkit_baseline_path, port_names)

    def check_build(self, needs_http):
        result = chromium.ChromiumPort.check_build(self, needs_http)
        result = self._check_wdiff_install() and result
        if not result:
            _log.error('For complete Mac build requirements, please see:')
            _log.error('')
            _log.error('    http://code.google.com/p/chromium/wiki/'
                       'MacBuildInstructions')
        return result

    def default_child_processes(self):
        # FIXME: we need to run single-threaded for now. See
        # https://bugs.webkit.org/show_bug.cgi?id=38553. Unfortunately this
        # routine is called right before the logger is configured, so if we
        # try to _log.warning(), it gets thrown away.
        import sys
        sys.stderr.write("Defaulting to one child - see https://bugs.webkit.org/show_bug.cgi?id=38553\n")
        return 1

    def driver_name(self):
        """name for this port's equivalent of DumpRenderTree."""
        if self._options.use_drt:
            return "DumpRenderTree"
        return "TestShell"

    def test_platform_name(self):
        # We use 'mac' instead of 'chromium-mac'
        return 'mac'

    def version(self):
        # FIXME: It's strange that this string is -version, not just version.
        os_version_string = platform.mac_ver()[0]  # e.g. "10.5.6"
        if not os_version_string:
            return '-leopard'
        release_version = int(os_version_string.split('.')[1])
        # we don't support 'tiger' or earlier releases
        if release_version == 5:
            return '-leopard'
        elif release_version == 6:
            return '-snowleopard'
        return ''

    #
    # PROTECTED METHODS
    #

    def _build_path(self, *comps):
        path = self.path_from_chromium_base('xcodebuild', *comps)
        if os.path.exists(path) or not self._options.use_drt:
            return path
        return self.path_from_webkit_base('WebKit', 'chromium', 'xcodebuild',
                                          *comps)

    def _check_wdiff_install(self):
        try:
            # We're ignoring the return and always returning True
            self._executive.run_command([self._path_to_wdiff()], error_handler=Executive.ignore_error)
        except OSError:
            _log.warning('wdiff not found. Install using MacPorts or some '
                         'other means')
        return True

    def _lighttpd_path(self, *comps):
        return self.path_from_chromium_base('third_party', 'lighttpd',
                                            'mac', *comps)

    def _path_to_apache(self):
        return '/usr/sbin/httpd'

    def _path_to_apache_config_file(self):
        return os.path.join(self.layout_tests_dir(), 'http', 'conf',
                            'apache2-httpd.conf')

    def _path_to_lighttpd(self):
        return self._lighttpd_path('bin', 'lighttpd')

    def _path_to_lighttpd_modules(self):
        return self._lighttpd_path('lib')

    def _path_to_lighttpd_php(self):
        return self._lighttpd_path('bin', 'php-cgi')

    def _path_to_driver(self, configuration=None):
        # FIXME: make |configuration| happy with case-sensitive file
        # systems.
        if not configuration:
            configuration = self._options.configuration
        return self._build_path(configuration, self.driver_name() + '.app',
            'Contents', 'MacOS', self.driver_name())

    def _path_to_helper(self):
        binary_name = 'layout_test_helper'
        if self._options.use_drt:
            binary_name = 'LayoutTestHelper'
        return self._build_path(self._options.configuration, binary_name)

    def _path_to_wdiff(self):
        return 'wdiff'

    def _shut_down_http_server(self, server_pid):
        """Shut down the lighttpd web server. Blocks until it's fully
        shut down.

        Args:
            server_pid: The process ID of the running server.
        """
        # server_pid is not set when "http_server.py stop" is run manually.
        if server_pid is None:
            # TODO(mmoss) This isn't ideal, since it could conflict with
            # lighttpd processes not started by http_server.py,
            # but good enough for now.
            self._executive.kill_all('lighttpd')
            self._executive.kill_all('httpd')
        else:
            try:
                os.kill(server_pid, signal.SIGTERM)
                # TODO(mmoss) Maybe throw in a SIGKILL just to be sure?
            except OSError:
                # Sometimes we get a bad PID (e.g. from a stale httpd.pid
                # file), so if kill fails on the given PID, just try to
                # 'killall' web servers.
                self._shut_down_http_server(None)
