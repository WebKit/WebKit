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

"""WebKit Mac implementation of the Port interface."""

import logging
import os
import pdb
import platform
import re
import shutil
import signal
import subprocess
import sys
import time
import webbrowser

import webkitpy.common.system.ospath as ospath
import webkitpy.layout_tests.port.server_process as server_process
from webkitpy.layout_tests.port.webkit import WebKitPort, WebKitDriver

_log = logging.getLogger("webkitpy.layout_tests.port.mac")


class MacPort(WebKitPort):
    """WebKit Mac implementation of the Port class."""

    def __init__(self, port_name=None, options=None):
        if port_name is None:
            port_name = 'mac' + self.version()
        WebKitPort.__init__(self, port_name, options)

    def default_num_dump_render_trees(self):
        # FIXME: new-run-webkit-tests is unstable on Mac running more than
        # four threads in parallel.
        # See https://bugs.webkit.org/show_bug.cgi?id=36622
        num_dump_render_trees = WebKitPort.default_num_dump_render_trees(self)
        if num_dump_render_trees > 4:
            return 4
        return num_dump_render_trees

    def baseline_search_path(self):
        dirs = []
        if self._name == 'mac-tiger':
            dirs.append(self._webkit_baseline_path(self._name))
        if self._name in ('mac-tiger', 'mac-leopard'):
            dirs.append(self._webkit_baseline_path('mac-leopard'))
        if self._name in ('mac-tiger', 'mac-leopard', 'mac-snowleopard'):
            dirs.append(self._webkit_baseline_path('mac-snowleopard'))
        dirs.append(self._webkit_baseline_path('mac'))
        return dirs

    def path_to_test_expectations_file(self):
        return self.path_from_webkit_base('LayoutTests', 'platform',
           'mac', 'test_expectations.txt')

    def _skipped_file_paths(self):
        # FIXME: This method will need to be made work for non-mac
        # platforms and moved into base.Port.
        skipped_files = []
        if self._name in ('mac-tiger', 'mac-leopard', 'mac-snowleopard'):
            skipped_files.append(os.path.join(
                self._webkit_baseline_path(self._name), 'Skipped'))
        skipped_files.append(os.path.join(self._webkit_baseline_path('mac'),
                                          'Skipped'))
        return skipped_files

    def test_platform_name(self):
        return 'mac' + self.version()

    def version(self):
        os_version_string = platform.mac_ver()[0]  # e.g. "10.5.6"
        if not os_version_string:
            return '-leopard'
        release_version = int(os_version_string.split('.')[1])
        if release_version == 4:
            return '-tiger'
        elif release_version == 5:
            return '-leopard'
        elif release_version == 6:
            return '-snowleopard'
        return ''

    def _tests_for_other_platforms(self):
        # The original run-webkit-tests builds up a "whitelist" of tests to
        # run, and passes that to DumpRenderTree. new-run-webkit-tests assumes
        # we run *all* tests and test_expectations.txt functions as a
        # blacklist.
        # FIXME: This list could be dynamic based on platform name and
        # pushed into base.Port.
        return [
            "platform/chromium",
            "platform/gtk",
            "platform/qt",
            "platform/win",
        ]

    def _tests_for_disabled_features(self):
        # FIXME: This should use the feature detection from
        # webkitperl/features.pm to match run-webkit-tests.
        # For now we hard-code a list of features known to be disabled on
        # the Mac platform.
        disabled_feature_tests = [
            "fast/xhtmlmp",
            "http/tests/wml",
            "mathml",
            "wml",
        ]
        # FIXME: webarchive tests expect to read-write from
        # -expected.webarchive files instead of .txt files.
        # This script doesn't know how to do that yet, so pretend they're
        # just "disabled".
        webarchive_tests = [
            "webarchive",
            "svg/webarchive",
            "http/tests/webarchive",
            "svg/custom/image-with-prefix-in-webarchive.svg",
        ]
        return disabled_feature_tests + webarchive_tests

    # FIXME: This doesn't have anything to do with WebKit.
    def _kill_process(self, pid):
        """Forcefully kill the process.

        Args:
        pid: The id of the process to be killed.
        """
        os.kill(pid, signal.SIGKILL)

    # FIXME: This doesn't have anything to do with WebKit.
    def _kill_all_process(self, process_name):
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

    # FIXME: This doesn't have anything to do with WebKit.
    def _shut_down_http_server(self, server_pid):
        """Shut down the lighttpd web server. Blocks until it's fully
        shut down.

        Args:
            server_pid: The process ID of the running server.
        """
        # server_pid is not set when "http_server.py stop" is run manually.
        if server_pid is None:
            # FIXME: This isn't ideal, since it could conflict with
            # lighttpd processes not started by http_server.py,
            # but good enough for now.
            self._kill_all_process('httpd')
        else:
            try:
                os.kill(server_pid, signal.SIGTERM)
                # FIXME: Maybe throw in a SIGKILL just to be sure?
            except OSError:
                # Sometimes we get a bad PID (e.g. from a stale httpd.pid
                # file), so if kill fails on the given PID, just try to
                # 'killall' web servers.
                self._shut_down_http_server(None)
