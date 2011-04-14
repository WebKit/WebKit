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
import platform
import signal

from webkitpy.layout_tests.port.webkit import WebKitPort

_log = logging.getLogger("webkitpy.layout_tests.port.mac")


def os_version(os_version_string=None, supported_versions=None):
    if not os_version_string:
        if hasattr(platform, 'mac_ver') and platform.mac_ver()[0]:
            os_version_string = platform.mac_ver()[0]
        else:
            # Make up something for testing.
            os_version_string = "10.5.6"
    release_version = int(os_version_string.split('.')[1])
    version_strings = {
        4: 'tiger',
        5: 'leopard',
        6: 'snowleopard',
    }
    assert release_version >= min(version_strings.keys())
    version_string = version_strings.get(release_version, 'future')
    if supported_versions:
        assert version_string in supported_versions
    return version_string


class MacPort(WebKitPort):
    """WebKit Mac implementation of the Port class."""
    # FIXME: 'wk2' probably shouldn't be a version, it should probably be
    # a modifier, like 'chromium-gpu' is to 'chromium'.
    SUPPORTED_VERSIONS = ('tiger', 'leopard', 'snowleopard', 'future', 'wk2')

    FALLBACK_PATHS = {
        'tiger': ['mac-tiger', 'mac-leopard', 'mac-snowleopard', 'mac'],
        'leopard': ['mac-leopard', 'mac-snowleopard', 'mac'],
        'snowleopard': ['mac-snowleopard', 'mac'],
        'future': ['mac'],
        'wk2': ['mac-wk2', 'mac'],
    }

    def __init__(self, port_name=None, os_version_string=None, **kwargs):
        port_name = port_name or 'mac'
        WebKitPort.__init__(self, port_name=port_name, **kwargs)
        if port_name == 'mac':
            self._version = os_version(os_version_string)
            self._name = port_name + '-' + self._version
        else:
            self._version = port_name[4:]
            assert self._version in self.SUPPORTED_VERSIONS
        self._operating_system = 'mac'
        if not hasattr(self._options, 'time-out-ms') or self._options.time_out_ms is None:
            self._options.time_out_ms = 35000

    def default_child_processes(self):
        # FIXME: new-run-webkit-tests is unstable on Mac running more than
        # four threads in parallel.
        # See https://bugs.webkit.org/show_bug.cgi?id=36622
        child_processes = WebKitPort.default_child_processes(self)
        if not self._multiprocessing_is_available and child_processes > 4:
            return 4
        return child_processes

    def baseline_path(self):
        if self.version() != 'future':
            return WebKitPort.baseline_path(self)

        assert(self._name[-7:] == '-future')
        return self._webkit_baseline_path(self._name[:-7])

    def baseline_search_path(self):
        return map(self._webkit_baseline_path, self.FALLBACK_PATHS[self._version])

    def path_to_test_expectations_file(self):
        return self.path_from_webkit_base('LayoutTests', 'platform',
           'mac', 'test_expectations.txt')

    def _skipped_file_paths(self):
        # FIXME: This method will need to be made work for non-mac
        # platforms and moved into base.Port.
        skipped_files = []
        if self._name in ('mac-leopard', 'mac-snowleopard'):
            skipped_files.append(self._filesystem.join(
                self._webkit_baseline_path(self._name), 'Skipped'))
        skipped_files.append(self._filesystem.join(self._webkit_baseline_path('mac'),
                                          'Skipped'))
        return skipped_files

    def _build_java_test_support(self):
        java_tests_path = self._filesystem.join(self.layout_tests_dir(), "java")
        build_java = ["/usr/bin/make", "-C", java_tests_path]
        if self._executive.run_command(build_java, return_exit_code=True):
            _log.error("Failed to build Java support files: %s" % build_java)
            return False
        return True

    def _check_port_build(self):
        return self._build_java_test_support()

    def _path_to_apache_config_file(self):
        return self._filesystem.join(self.layout_tests_dir(), 'http', 'conf',
                                     'apache2-httpd.conf')

    def _path_to_webcore_library(self):
        return self._build_path('WebCore.framework/Versions/A/WebCore')

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
            self._executive.kill_all('httpd')
        else:
            try:
                os.kill(server_pid, signal.SIGTERM)
                # FIXME: Maybe throw in a SIGKILL just to be sure?
            except OSError:
                # Sometimes we get a bad PID (e.g. from a stale httpd.pid
                # file), so if kill fails on the given PID, just try to
                # 'killall' web servers.
                self._shut_down_http_server(None)
