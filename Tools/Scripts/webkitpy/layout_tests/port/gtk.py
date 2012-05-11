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

from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.layout_tests.port.server_process import ServerProcess
from webkitpy.layout_tests.port.webkit import WebKitDriver, WebKitPort
from webkitpy.layout_tests.port.pulseaudio_sanitizer import PulseAudioSanitizer
from webkitpy.layout_tests.port.xvfbdriver import XvfbDriver
from webkitpy.common.system.executive import Executive

class GtkPort(WebKitPort, PulseAudioSanitizer):
    port_name = "gtk"

    def _port_flag_for_scripts(self):
        return "--gtk"

    def _driver_class(self):
        return XvfbDriver

    def setup_test_run(self):
        self._unload_pulseaudio_module()

    def clean_up_test_run(self):
        self._restore_pulseaudio_module()

    def setup_environ_for_server(self, server_name=None):
        environment = WebKitPort.setup_environ_for_server(self, server_name)
        environment['GTK_MODULES'] = 'gail'
        environment['GSETTINGS_BACKEND'] = 'memory'
        environment['LIBOVERLAY_SCROLLBAR'] = '0'
        environment['TEST_RUNNER_INJECTED_BUNDLE_FILENAME'] = self._build_path('Libraries', 'libTestRunnerInjectedBundle.la')
        environment['TEST_RUNNER_TEST_PLUGIN_PATH'] = self._build_path('TestNetscapePlugin', '.libs')
        environment['WEBKIT_INSPECTOR_PATH'] = self._build_path('Programs', 'resources', 'inspector')
        environment['AUDIO_RESOURCES_PATH'] = self._filesystem.join(self._config.webkit_base_dir(),
                                                                    'Source', 'WebCore', 'platform',
                                                                    'audio', 'resources')
        if self.get_option('webkit_test_runner'):
            # FIXME: This is a workaround to ensure that testing with WebKitTestRunner is started with
            # a non-existing cache. This should be removed when (and if) it will be possible to properly
            # set the cache directory path through a WebKitWebContext.
            environment['XDG_CACHE_HOME'] = self._filesystem.join(self.results_directory(), 'appcache')
        return environment

    def _generate_all_test_configurations(self):
        configurations = []
        for build_type in self.ALL_BUILD_TYPES:
            configurations.append(TestConfiguration(version=self._version, architecture='x86', build_type=build_type))
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

    # FIXME: We should find a way to share this implmentation with Gtk,
    # or teach run-launcher how to call run-safari and move this down to WebKitPort.
    def show_results_html_file(self, results_filename):
        run_launcher_args = ["file://%s" % results_filename]
        if self.get_option('webkit_test_runner'):
            run_launcher_args.append('-2')
        # FIXME: old-run-webkit-tests also added ["-graphicssystem", "raster", "-style", "windows"]
        # FIXME: old-run-webkit-tests converted results_filename path for cygwin.
        self._run_script("run-launcher", run_launcher_args)

    def _get_gdb_output(self, coredump_path):
        cmd = ['gdb', '-ex', 'thread apply all bt', '--batch', str(self._path_to_driver()), coredump_path]
        proc = subprocess.Popen(cmd, stdin=None, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        proc.wait()
        errors = [l.strip().decode('utf8', 'ignore') for l in proc.stderr.readlines()]
        trace = proc.stdout.read().decode('utf8', 'ignore')
        return (trace, errors)

    def _get_crash_log(self, name, pid, stdout, stderr, newer_than):
        pid_representation = str(pid or '<unknown>')
        log_directory = os.environ.get("WEBKIT_CORE_DUMPS_DIRECTORY")
        errors = []
        crash_log = ''
        expected_crash_dump_filename = "core-pid_%s-_-process_%s" % (pid_representation, name)

        def match_filename(filesystem, directory, filename):
            if pid:
                return filename == expected_crash_dump_filename
            return filename.find(name) > -1

        if log_directory:
            dumps = self._filesystem.files_under(log_directory, file_filter=match_filename)
            if dumps:
                # Get the most recent coredump matching the pid and/or process name.
                coredump_path = list(reversed(sorted(dumps)))[0]
                if not newer_than or self._filesystem.mtime(coredump_path) > newer_than:
                    crash_log, errors = self._get_gdb_output(coredump_path)

        stderr_lines = errors + (stderr or '<empty>').decode('utf8', 'ignore').splitlines()
        errors_str = '\n'.join(('STDERR: ' + l) for l in stderr_lines)
        if not crash_log:
            if not log_directory:
                log_directory = "/path/to/coredumps"
            core_pattern = os.path.join(log_directory, "core-pid_%p-_-process_%e")
            crash_log = """\
Coredump %(expected_crash_dump_filename)s not found. To enable crash logs:

- run this command as super-user: echo "%(core_pattern)s" > /proc/sys/kernel/core_pattern
- enable core dumps: ulimit -c unlimited
- set the WEBKIT_CORE_DUMPS_DIRECTORY environment variable: export WEBKIT_CORE_DUMPS_DIRECTORY=%(log_directory)s

""" % locals()

        return """\
Crash log for %(name)s (pid %(pid_representation)s):

%(crash_log)s
%(errors_str)s""" % locals()
