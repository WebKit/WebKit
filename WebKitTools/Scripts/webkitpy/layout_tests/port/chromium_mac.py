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

import os
import platform
import signal
import subprocess

import chromium


class ChromiumMacPort(chromium.ChromiumPort):
    """Chromium Mac implementation of the Port class."""

    def __init__(self, port_name=None, options=None):
        if port_name is None:
            port_name = 'chromium-mac'
        chromium.ChromiumPort.__init__(self, port_name, options)

    def baseline_search_path(self):
        return [self.baseline_path(),
                self._webkit_baseline_path('mac' + self.version()),
                self._webkit_baseline_path('mac')]

    def check_sys_deps(self):
        # We have no specific platform dependencies.
        return True

    def num_cores(self):
        return int(subprocess.Popen(['sysctl','-n','hw.ncpu'],
                                    stdout=subprocess.PIPE).stdout.read())

    def test_platform_name(self):
        # We use 'mac' instead of 'chromium-mac'
        return 'mac'

    def version(self):
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
        return self.path_from_chromium_base('xcodebuild', self._options.target,
                                            *comps)

    def _lighttpd_path(self, *comps):
        return self.path_from_chromium_base('third_party', 'lighttpd',
                                            'mac', *comps)

    def _kill_process(self, pid):
        """Forcefully kill the process.

        Args:
            pid: The id of the process to be killed.
        """
        os.kill(pid, signal.SIGKILL)

    def _kill_all_process(self, process_name):
        """Kill any processes running under this name."""
        # On Mac OS X 10.6, killall has a new constraint: -SIGNALNAME or
        # -SIGNALNUMBER must come first.  Example problem:
        #   $ killall -u $USER -TERM lighttpd
        #   killall: illegal option -- T
        # Use of the earlier -TERM placement is just fine on 10.5.
        null = open(os.devnull)
        subprocess.call(['killall', '-TERM', '-u', os.getenv('USER'),
                        process_name], stderr=null)
        null.close()

    def _path_to_apache(self):
        return '/usr/sbin/httpd'

    def _path_to_apache_config_file(self):
        return os.path.join(self.layout_tests_dir(), 'http', 'conf',
                            'apache2-httpd.conf')

    def _path_to_lighttpd(self):
        return self._lighttp_path('bin', 'lighttp')

    def _path_to_lighttpd_modules(self):
        return self._lighttp_path('lib')

    def _path_to_lighttpd_php(self):
        return self._lighttpd_path('bin', 'php-cgi')

    def _path_to_driver(self):
        # TODO(pinkerton): make |target| happy with case-sensitive file
        # systems.
        return self._build_path('TestShell.app', 'Contents', 'MacOS', 
                                'TestShell')

    def _path_to_helper(self):
        return self._build_path('layout_test_helper')

    def _path_to_image_diff(self):
        return self._build_path('image_diff')

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
            self._kill_all_process('lighttpd')
            self._kill_all_process('httpd')
        else:
            try:
                os.kill(server_pid, signal.SIGTERM)
                # TODO(mmoss) Maybe throw in a SIGKILL just to be sure?
            except OSError:
                # Sometimes we get a bad PID (e.g. from a stale httpd.pid
                # file), so if kill fails on the given PID, just try to
                # 'killall' web servers.
                self._shut_down_http_server(None)
