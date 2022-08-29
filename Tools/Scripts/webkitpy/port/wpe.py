# -*- coding: utf-8 -*-
# Copyright (C) 2014-2021 Igalia S.L.  All rights reserved.
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
import shlex

from webkitpy.common.system import path
from webkitpy.common.memoized import memoized
from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.port.base import Port
from webkitpy.port.headlessdriver import HeadlessDriver
from webkitpy.port.linux_get_crash_log import GDBCrashLogGenerator

from webkitcorepy import decorators


class WPEPort(Port):
    port_name = "wpe"

    def __init__(self, *args, **kwargs):
        super(WPEPort, self).__init__(*args, **kwargs)

        self._display_server = self.get_option("display_server")
        if self._display_server == 'xvfb':
            # While not supported by WPE, xvfb is used as the default value in the main scripts
            self._display_server = 'headless'
        if self._should_use_jhbuild():
            self._jhbuild_wrapper = [self.path_from_webkit_base('Tools', 'jhbuild', 'jhbuild-wrapper'), '--wpe', 'run']
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

    def _port_flag_for_scripts(self):
        return "--wpe"

    @memoized
    def _driver_class(self):
        return HeadlessDriver

    def setup_environ_for_server(self, server_name=None):
        environment = super(WPEPort, self).setup_environ_for_server(server_name)
        environment['G_DEBUG'] = 'fatal-criticals'
        environment['GSETTINGS_BACKEND'] = 'memory'
        environment['TEST_RUNNER_INJECTED_BUNDLE_FILENAME'] = self._build_path('lib', 'libTestRunnerInjectedBundle.so')
        environment['TEST_RUNNER_TEST_PLUGIN_PATH'] = self._build_path('lib', 'plugins')
        environment['WEBKIT_EXEC_PATH'] = self._build_path('bin')
        self._copy_value_from_environ_if_set(environment, 'WEBKIT_OUTPUTDIR')
        self._copy_value_from_environ_if_set(environment, 'WEBKIT_JHBUILD')
        self._copy_value_from_environ_if_set(environment, 'WEBKIT_TOP_LEVEL')
        self._copy_value_from_environ_if_set(environment, 'WEBKIT_DEBUG')
        self._copy_value_from_environ_if_set(environment, 'AT_SPI_BUS_ADDRESS')
        self._copy_value_from_environ_if_set(environment, 'LIBGL_ALWAYS_SOFTWARE')
        self._copy_value_from_environ_if_set(environment, 'PULSE_SERVER')
        self._copy_value_from_environ_if_set(environment, 'PULSE_CLIENTCONFIG')
        self._copy_value_from_environ_if_set(environment, 'XR_RUNTIME_JSON')
        self._copy_value_from_environ_if_set(environment, 'WEBKIT_GST_USE_PLAYBIN3')
        self._copy_value_from_environ_if_set(environment, 'BREAKPAD_MINIDUMP_DIR')
        for gst_variable in ('DEBUG', 'DEBUG_DUMP_DOT_DIR', 'DEBUG_FILE', 'DEBUG_NO_COLOR',
                             'PLUGIN_SCANNER', 'PLUGIN_PATH', 'PLUGIN_SYSTEM_PATH', 'REGISTRY',
                             'PLUGIN_PATH_1_0'):
            self._copy_value_from_environ_if_set(environment, 'GST_%s' % gst_variable)

        gst_feature_rank_override = os.environ.get('GST_PLUGIN_FEATURE_RANK')
        environment['GST_PLUGIN_FEATURE_RANK'] = 'fakeaudiosink:max'
        if gst_feature_rank_override:
            environment['GST_PLUGIN_FEATURE_RANK'] += ',%s' % gst_feature_rank_override

        return environment

    def show_results_html_file(self, results_filename):
        self.run_minibrowser([path.abspath_to_uri(self.host.platform, results_filename)])

    def check_sys_deps(self):
        return super(WPEPort, self).check_sys_deps() and self._driver_class().check_driver(self)

    def _generate_all_test_configurations(self):
        configurations = []
        for build_type in self.ALL_BUILD_TYPES:
            configurations.append(TestConfiguration(version=self.version_name(), architecture='x86', build_type=build_type))
        return configurations

    def _path_to_driver(self):
        return self._built_executables_path(self.driver_name())

    @decorators.Memoize()
    def _path_to_image_diff(self):
        return self._built_executables_path('ImageDiff')

    def _path_to_default_image_diff(self):
        return self._path_to_image_diff()

    def _search_paths(self):
        return [self.port_name, 'glib', 'wk2'] + self.get_option("additional_platform_directory", [])

    def default_baseline_search_path(self, **kwargs):
        return list(map(self._webkit_baseline_path, self._search_paths()))

    def _port_specific_expectations_files(self, **kwargs):
        return list(map(lambda x: self._filesystem.join(self._webkit_baseline_path(x), 'TestExpectations'), reversed(self._search_paths())))

    def test_expectations_file_position(self):
        # WPE port baseline search path is wpe -> glib -> wk2 -> generic, so port test expectations file is at third to last position.
        return 3

    def _get_crash_log(self, name, pid, stdout, stderr, newer_than, target_host=None):
        return GDBCrashLogGenerator(self._executive, name, pid, newer_than,
                                    self._filesystem, self._path_to_driver, self.port_name, self.get_option('configuration')).generate_crash_log(stdout, stderr)

    def configuration_for_upload(self, host=None):
        configuration = super(WPEPort, self).configuration_for_upload(host=host)
        configuration['platform'] = 'WPE'
        return configuration

    def cog_path_to(self, *components):
        return self._build_path('Tools', 'cog-prefix', 'src', 'cog-build', *components)

    def browser_name(self):
        """Returns the lower case name of the browser to be used (Cog or MiniBrowser)

        Users can select between both with the environment variable WPE_BROWSER
        """
        browser = os.environ.get("WPE_BROWSER", "").lower()
        if browser in ("cog", "minibrowser"):
            return browser

        if browser:
            print("Unknown browser {}. Defaulting to Cog and MiniBrowser selection".format(browser))

        if self._filesystem.isfile(self.cog_path_to('launcher', 'cog')):
            return "cog"
        return "minibrowser"

    def browser_env(self):
        env = os.environ.copy()

        if self.browser_name() == "cog":
            env.update({'WEBKIT_EXEC_PATH': self._build_path('bin'),
                        'COG_MODULEDIR': self.cog_path_to('platform'),
                        'WEBKIT_INJECTED_BUNDLE_PATH': self._build_path('lib')})

        return env

    def run_minibrowser(self, args):
        env = None
        miniBrowser = None

        if self.browser_name() == "cog":
            miniBrowser = self.cog_path_to('launcher', 'cog')
            if not self._filesystem.isfile(miniBrowser):
                print("Cog not found 😢. If you wish to enable it, rebuild with `-DENABLE_COG=ON`. Falling back to good old MiniBrowser")
                miniBrowser = None
            else:
                print("Using Cog as MiniBrowser")
                env = self.browser_env()
                has_platform_arg = any((a == "-P" or a.startswith("--platform=") for a in args))
                if not has_platform_arg:
                    args.insert(0, "--platform=gtk4")

        if not miniBrowser:
            print("Using default MiniBrowser")
            miniBrowser = self._build_path('bin', 'MiniBrowser')
            if not self._filesystem.isfile(miniBrowser):
                print("%s not found... Did you run build-webkit?" % miniBrowser)
                return 1
        command = [miniBrowser]
        if os.environ.get("WEBKIT_MINI_BROWSER_PREFIX"):
            command = shlex.split(os.environ["WEBKIT_MINI_BROWSER_PREFIX"]) + command

        if self._should_use_jhbuild():
            command = self._jhbuild_wrapper + command
        return self._executive.run_command(command + args, cwd=self.webkit_base(), stdout=None, return_stderr=False, decode_output=False, env=env)
