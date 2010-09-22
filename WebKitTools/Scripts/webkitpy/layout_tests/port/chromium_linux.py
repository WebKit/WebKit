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

"""Chromium Linux implementation of the Port interface."""

import logging
import os
import signal

import chromium

_log = logging.getLogger("webkitpy.layout_tests.port.chromium_linux")


class ChromiumLinuxPort(chromium.ChromiumPort):
    """Chromium Linux implementation of the Port class."""

    def __init__(self, **kwargs):
        kwargs.setdefault('port_name', 'chromium-linux')
        chromium.ChromiumPort.__init__(self, **kwargs)

    def baseline_search_path(self):
        port_names = ["chromium-linux", "chromium-win", "chromium", "win", "mac"]
        return map(self._webkit_baseline_path, port_names)

    def check_build(self, needs_http):
        result = chromium.ChromiumPort.check_build(self, needs_http)
        if needs_http:
            if self._options.use_apache:
                result = self._check_apache_install() and result
            else:
                result = self._check_lighttpd_install() and result
        result = self._check_wdiff_install() and result

        if not result:
            _log.error('For complete Linux build requirements, please see:')
            _log.error('')
            _log.error('    http://code.google.com/p/chromium/wiki/'
                       'LinuxBuildInstructions')
        return result

    def test_platform_name(self):
        # We use 'linux' instead of 'chromium-linux' in test_expectations.txt.
        return 'linux'

    def version(self):
        # We don't have different versions on linux.
        return ''

    #
    # PROTECTED METHODS
    #

    def _build_path(self, *comps):
        base = self.path_from_chromium_base()
        if os.path.exists(os.path.join(base, 'sconsbuild')):
            return os.path.join(base, 'sconsbuild', *comps)
        if os.path.exists(os.path.join(base, 'out', *comps)) or not self._options.use_drt:
            return os.path.join(base, 'out', *comps)
        base = self.path_from_webkit_base()
        if os.path.exists(os.path.join(base, 'sconsbuild')):
            return os.path.join(base, 'sconsbuild', *comps)
        return os.path.join(base, 'out', *comps)

    def _check_apache_install(self):
        result = chromium.check_file_exists(self._path_to_apache(),
            "apache2")
        result = chromium.check_file_exists(self._path_to_apache_config_file(),
            "apache2 config file") and result
        if not result:
            _log.error('    Please install using: "sudo apt-get install '
                       'apache2 libapache2-mod-php5"')
            _log.error('')
        return result

    def _check_lighttpd_install(self):
        result = chromium.check_file_exists(
            self._path_to_lighttpd(), "LigHTTPd executable")
        result = chromium.check_file_exists(self._path_to_lighttpd_php(),
            "PHP CGI executable") and result
        result = chromium.check_file_exists(self._path_to_lighttpd_modules(),
            "LigHTTPd modules") and result
        if not result:
            _log.error('    Please install using: "sudo apt-get install '
                       'lighttpd php5-cgi"')
            _log.error('')
        return result

    def _check_wdiff_install(self):
        result = chromium.check_file_exists(self._path_to_wdiff(), 'wdiff')
        if not result:
            _log.error('    Please install using: "sudo apt-get install '
                       'wdiff"')
            _log.error('')
        # FIXME: The ChromiumMac port always returns True.
        return result

    def _path_to_apache(self):
        if self._is_redhat_based():
            return '/usr/sbin/httpd'
        else:
            return '/usr/sbin/apache2'

    def _path_to_apache_config_file(self):
        if self._is_redhat_based():
            config_name = 'fedora-httpd.conf'
        else:
            config_name = 'apache2-debian-httpd.conf'

        return os.path.join(self.layout_tests_dir(), 'http', 'conf',
                            config_name)

    def _path_to_lighttpd(self):
        return "/usr/sbin/lighttpd"

    def _path_to_lighttpd_modules(self):
        return "/usr/lib/lighttpd"

    def _path_to_lighttpd_php(self):
        return "/usr/bin/php-cgi"

    def _path_to_driver(self, configuration=None):
        if not configuration:
            configuration = self._options.configuration
        binary_name = 'test_shell'
        if self._options.use_drt:
            binary_name = 'DumpRenderTree'
        return self._build_path(configuration, binary_name)

    def _path_to_helper(self):
        return None

    def _path_to_wdiff(self):
        if self._is_redhat_based():
            return '/usr/bin/dwdiff'
        else:
            return '/usr/bin/wdiff'

    def _is_redhat_based(self):
        return os.path.exists(os.path.join('/etc', 'redhat-release'))

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
            self._executive.kill_all("lighttpd")
            self._executive.kill_all("apache2")
        else:
            try:
                os.kill(server_pid, signal.SIGTERM)
                # TODO(mmoss) Maybe throw in a SIGKILL just to be sure?
            except OSError:
                # Sometimes we get a bad PID (e.g. from a stale httpd.pid
                # file), so if kill fails on the given PID, just try to
                # 'killall' web servers.
                self._shut_down_http_server(None)
