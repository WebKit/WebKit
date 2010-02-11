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

"""Chromium Win implementation of the Port interface."""

import os
import platform
import signal
import subprocess
import sys

import chromium


class ChromiumWinPort(chromium.ChromiumPort):
    """Chromium Win implementation of the Port class."""

    def __init__(self, port_name=None, options=None):
        if port_name is None:
            port_name = 'chromium-win' + self.version()
        chromium.ChromiumPort.__init__(self, port_name, options)

    def baseline_search_path(self):
        dirs = []
        if self._name == 'chromium-win-xp':
            dirs.append(self._chromium_baseline_path(self._name))
        if self._name in ('chromium-win-xp', 'chromium-win-vista'):
            dirs.append(self._chromium_baseline_path('chromium-win-vista'))
        dirs.append(self._chromium_baseline_path('chromium-win'))
        dirs.append(self._webkit_baseline_path('win'))
        dirs.append(self._webkit_baseline_path('mac'))
        return dirs

    def check_sys_deps(self):
        # TODO(dpranke): implement this
        return True

    def get_absolute_path(self, filename):
        """Return the absolute path in unix format for the given filename."""
        abspath = os.path.abspath(filename)
        return abspath.replace('\\', '/')

    def num_cores(self):
        return int(os.environ.get('NUMBER_OF_PROCESSORS', 1))

    def relative_test_filename(self, filename):
        path = filename[len(self.layout_tests_dir()) + 1:]
        return path.replace('\\', '/')

    def test_platform_name(self):
        # We return 'win-xp', not 'chromium-win-xp' here, for convenience.
        return 'win' + self.version()

    def version(self):
        winver = sys.getwindowsversion()
        if winver[0] == 6 and (winver[1] == 1):
            return '-7'
        if winver[0] == 6 and (winver[1] == 0):
            return '-vista'
        if winver[0] == 5 and (winver[1] == 1 or winver[1] == 2):
            return '-xp'
        return ''

    #
    # PROTECTED ROUTINES
    #

    def _build_path(self, *comps):
        # FIXME(dpranke): allow for builds under 'chrome' as well.
        return self.path_from_chromium_base('webkit', self._options.target,
                                            *comps)

    def _lighttpd_path(self, *comps):
        return self.path_from_chromium_base('third_party', 'lighttpd', 'win',
                                            *comps)

    def _kill_process(self, pid):
        """Forcefully kill the process.

        Args:
        pid: The id of the process to be killed.
        """
        subprocess.call(('taskkill.exe', '/f', '/pid', str(pid)),
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE)

    def _path_to_apache(self):
        return self.path_from_chromium_base('third_party', 'cygwin', 'usr',
                                            'sbin', 'httpd')

    def _path_to_apache_config_file(self):
        return os.path.join(self.layout_tests_dir(), 'http', 'conf',
                            'cygwin-httpd.conf')

    def _path_to_lighttpd(self):
        return self._lighttpd_path('LightTPD.exe')

    def _path_to_lighttpd_modules(self):
        return self._lighttpd_path('lib')

    def _path_to_lighttpd_php(self):
        return self._lighttpd_path('php5', 'php-cgi.exe')

    def _path_to_driver(self):
        return self._build_path('test_shell.exe')

    def _path_to_helper(self):
        return self._build_path('layout_test_helper.exe')

    def _path_to_image_diff(self):
        return self._build_path('image_diff.exe')

    def _path_to_wdiff(self):
        return self.path_from_chromium_base('third_party', 'cygwin', 'bin',
                                            'wdiff.exe')

    def _shut_down_http_server(self, server_pid):
        """Shut down the lighttpd web server. Blocks until it's fully
        shut down.

        Args:
            server_pid: The process ID of the running server.
        """
        subprocess.Popen(('taskkill.exe', '/f', '/im', 'LightTPD.exe'),
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE).wait()
        subprocess.Popen(('taskkill.exe', '/f', '/im', 'httpd.exe'),
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE).wait()
