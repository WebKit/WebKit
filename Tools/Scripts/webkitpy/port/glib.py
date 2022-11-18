# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2013 Samsung Electronics.  All rights reserved.
# Copyright (C) 2017-2022 Igalia S.L. All rights reserved.
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

import os
import uuid

from webkitpy.port.base import Port
from webkitpy.port.leakdetector_valgrind import LeakDetectorValgrind
from webkitpy.port.linux_get_crash_log import GDBCrashLogGenerator


class GLibPort(Port):

    def __init__(self, *args, **kwargs):
        super(GLibPort, self).__init__(*args, **kwargs)
        self._display_server = self.get_option("display_server")

        if self.get_option("leaks"):
            self._leakdetector = LeakDetectorValgrind(self._executive, self._filesystem, self.results_directory())
            if not self.get_option("wrapper"):
                raise ValueError('use --wrapper=\"valgrind\" for memory leak detection')

        if self._should_use_jhbuild():
            self._jhbuild_wrapper = [self.path_from_webkit_base('Tools', 'jhbuild', 'jhbuild-wrapper'), self._port_flag_for_scripts(), 'run']
            if self.get_option('wrapper'):
                self.set_option('wrapper', ' '.join(self._jhbuild_wrapper) + ' ' + self.get_option('wrapper'))
            else:
                self.set_option_default('wrapper', ' '.join(self._jhbuild_wrapper))

    def default_timeout_ms(self):
        default_timeout = 15000
        # Starting an application under Valgrind takes a lot longer than normal
        # so increase the timeout (empirically 10x is enough to avoid timeouts).
        multiplier = 10 if self.get_option("leaks") else 1
        # Debug builds are slower (no compiler optimizations are used).
        if self.get_option('configuration') == 'Debug':
            multiplier *= 2
        return multiplier * default_timeout

    def _built_executables_path(self, *path):
        return self._build_path(*(('bin',) + path))

    def _built_libraries_path(self, *path):
        return self._build_path(*(('lib',) + path))

    def _prepend_to_env_value(self, new_value, current_value):
        if len(current_value) > 0:
            return new_value + ":" + current_value
        return new_value

    def setup_test_run(self, device_type=None):
        super(GLibPort, self).setup_test_run(device_type)

        if self.get_option("leaks"):
            self._leakdetector.clean_leaks_files_from_results_directory()

    def setup_environ_for_server(self, server_name=None):
        environment = super(GLibPort, self).setup_environ_for_server(server_name)
        environment['G_DEBUG'] = 'fatal-criticals'
        environment['GSETTINGS_BACKEND'] = 'memory'

        environment['TEST_RUNNER_INJECTED_BUNDLE_FILENAME'] = self._build_path('lib', 'libTestRunnerInjectedBundle.so')
        environment['TEST_RUNNER_TEST_PLUGIN_PATH'] = self._build_path('lib', 'plugins')
        environment['WEBKIT_EXEC_PATH'] = self._build_path('bin')
        environment['WEBKIT_FONTS_CONF_DIR'] = self.path_from_webkit_base('Tools', 'WebKitTestRunner', 'gtk', 'fonts')
        environment['LD_LIBRARY_PATH'] = self._prepend_to_env_value(self._build_path('lib'), environment.get('LD_LIBRAY_PATH', ''))
        self._copy_value_from_environ_if_set(environment, 'LIBGL_ALWAYS_SOFTWARE')
        self._copy_value_from_environ_if_set(environment, 'WEBKIT_OUTPUTDIR')
        self._copy_value_from_environ_if_set(environment, 'WEBKIT_JHBUILD')
        self._copy_value_from_environ_if_set(environment, 'WEBKIT_TOP_LEVEL')
        self._copy_value_from_environ_if_set(environment, 'WEBKIT_DEBUG')
        self._copy_value_from_environ_if_set(environment, 'WEBKIT_GST_USE_PLAYBIN3')
        self._copy_value_from_environ_if_set(environment, 'AT_SPI_BUS_ADDRESS')
        for gst_variable in ('DEBUG', 'DEBUG_DUMP_DOT_DIR', 'DEBUG_FILE', 'DEBUG_NO_COLOR',
                             'PLUGIN_SCANNER', 'PLUGIN_PATH', 'PLUGIN_SYSTEM_PATH', 'REGISTRY',
                             'PLUGIN_PATH_1_0'):
            self._copy_value_from_environ_if_set(environment, 'GST_%s' % gst_variable)

        gst_feature_rank_override = os.environ.get('GST_PLUGIN_FEATURE_RANK')
        environment['GST_PLUGIN_FEATURE_RANK'] = 'fakeaudiosink:max'
        if gst_feature_rank_override:
            environment['GST_PLUGIN_FEATURE_RANK'] += ',%s' % gst_feature_rank_override

        if self.get_option("leaks"):
            # Turn off GLib memory optimisations https://wiki.gnome.org/Valgrind.
            environment['G_SLICE'] = 'always-malloc'
            environment['G_DEBUG'] += ',gc-friendly'
            # Turn off bmalloc when running under Valgrind, see https://bugs.webkit.org/show_bug.cgi?id=177745
            environment['Malloc'] = '1'
            xmlfilename = "".join(("drt-%p-", uuid.uuid1().hex, "-leaks.xml"))
            xmlfile = os.path.join(self.results_directory(), xmlfilename)
            suppressionsfile = self.path_from_webkit_base('Tools', 'Scripts', 'valgrind', 'suppressions.txt')
            environment['VALGRIND_OPTS'] = \
                "--tool=memcheck " \
                "--num-callers=40 " \
                "--demangle=no " \
                "--trace-children=no " \
                "--smc-check=all-non-file " \
                "--leak-check=yes " \
                "--leak-resolution=high " \
                "--show-possibly-lost=no " \
                "--show-reachable=no " \
                "--leak-check=full " \
                "--undef-value-errors=no " \
                "--gen-suppressions=all " \
                "--xml=yes " \
                "--xml-file=%s " \
                "--suppressions=%s" % (xmlfile, suppressionsfile)

        return environment

    def setup_environ_for_minibrowser(self):
        env = os.environ.copy()
        env['WEBKIT_EXEC_PATH'] = self._build_path('bin')
        env['WEBKIT_INJECTED_BUNDLE_PATH'] = self._build_path('lib')
        env['LD_LIBRARY_PATH'] = self._prepend_to_env_value(self._build_path('lib'), env.get('LD_LIBRAY_PATH', ''))
        return env

    def _get_crash_log(self, name, pid, stdout, stderr, newer_than, target_host=None):
        return GDBCrashLogGenerator(self._executive, name, pid, newer_than,
                                    self._filesystem, self._path_to_driver, self.port_name, self.get_option('configuration')).generate_crash_log(stdout, stderr)
