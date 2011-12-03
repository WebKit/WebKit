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

from __future__ import with_statement

"""WebKit Gtk implementation of the Port interface."""

import logging
import os
import signal
import subprocess

from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.layout_tests.port.server_process import ServerProcess
from webkitpy.layout_tests.port.webkit import WebKitDriver, WebKitPort


_log = logging.getLogger(__name__)


class GtkDriver(WebKitDriver):
    def _start(self):
        # Use even displays for pixel tests and odd ones otherwise. When pixel tests are disabled,
        # DriverProxy creates two drivers, one for normal and the other for ref tests. Both have
        # the same worker number, so this prevents them from using the same Xvfb instance.
        display_id = self._worker_number * 2 + 1
        if self._pixel_tests:
            display_id += 1
        run_xvfb = ["Xvfb", ":%d" % (display_id), "-screen",  "0", "800x600x24", "-nolisten", "tcp"]
        with open(os.devnull, 'w') as devnull:
            self._xvfb_process = subprocess.Popen(run_xvfb, stderr=devnull)
        server_name = self._port.driver_name()
        environment = self._port.setup_environ_for_server(server_name)
        # We must do this here because the DISPLAY number depends on _worker_number
        environment['DISPLAY'] = ":%d" % (display_id)
        self._server_process = ServerProcess(self._port, server_name, self.cmd_line(), environment)

    def stop(self):
        WebKitDriver.stop(self)
        if getattr(self, '_xvfb_process', None):
            # FIXME: This should use Executive.kill_process
            os.kill(self._xvfb_process.pid, signal.SIGTERM)
            self._xvfb_process.wait()
            self._xvfb_process = None

    def cmd_line(self):
        wrapper_path = self._port.path_from_webkit_base("Tools", "gtk", "run-with-jhbuild")
        return [wrapper_path] + WebKitDriver.cmd_line(self)


class GtkPort(WebKitPort):
    port_name = "gtk"

    def __init__(self, host, **kwargs):
        WebKitPort.__init__(self, host, **kwargs)
        self._version = self.port_name

    def _port_flag_for_scripts(self):
        return "--gtk"

    def _driver_class(self):
        return GtkDriver

    def setup_environ_for_server(self, server_name=None):
        environment = WebKitPort.setup_environ_for_server(self, server_name)
        environment['GTK_MODULES'] = 'gail'
        environment['LIBOVERLAY_SCROLLBAR'] = '0'
        environment['TEST_RUNNER_INJECTED_BUNDLE_FILENAME'] = self._build_path('Libraries', 'libTestRunnerInjectedBundle.la')
        environment['TEST_RUNNER_TEST_PLUGIN_PATH'] = self._build_path('TestNetscapePlugin', '.libs')
        environment['WEBKIT_INSPECTOR_PATH'] = self._build_path('Programs', 'resources', 'inspector')
        environment['WEBKIT_TOP_LEVEL'] = self._config.webkit_base_dir()
        return environment

    def _generate_all_test_configurations(self):
        configurations = []
        for build_type in self.ALL_BUILD_TYPES:
            configurations.append(TestConfiguration(version=self._version, architecture='x86', build_type=build_type, graphics_type='cpu'))
        return configurations

    def _path_to_driver(self):
        return self._build_path('Programs', self.driver_name())

    def _path_to_image_diff(self):
        return self._build_path('Programs', 'ImageDiff')

    def check_build(self, needs_http):
        return self._check_driver()

    def _path_to_apache(self):
        if self._is_redhat_based():
            return '/usr/sbin/httpd'
        else:
            return '/usr/sbin/apache2'

    def _path_to_wdiff(self):
        if self._is_redhat_based():
            return '/usr/bin/dwdiff'
        else:
            return '/usr/bin/wdiff'

    def _path_to_webcore_library(self):
        gtk_library_names = [
            "libwebkitgtk-1.0.so",
            "libwebkitgtk-3.0.so",
            "libwebkit2gtk-1.0.so",
        ]

        for library in gtk_library_names:
            full_library = self._build_path(".libs", library)
            if self._filesystem.isfile(full_library):
                return full_library
        return None

    def _runtime_feature_list(self):
        return None

    # FIXME: We should find a way to share this implmentation with Gtk,
    # or teach run-launcher how to call run-safari and move this down to WebKitPort.
    def show_results_html_file(self, results_filename):
        run_launcher_args = ["file://%s" % results_filename]
        if self.get_option('webkit_test_runner'):
            run_launcher_args.append('-2')
        # FIXME: old-run-webkit-tests also added ["-graphicssystem", "raster", "-style", "windows"]
        # FIXME: old-run-webkit-tests converted results_filename path for cygwin.
        self._run_script("run-launcher", run_launcher_args)
