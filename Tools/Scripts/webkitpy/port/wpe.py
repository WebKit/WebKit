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

import logging
import os
import shlex

from webkitpy.common.system import path
from webkitpy.common.memoized import memoized
from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.port.glib import GLibPort
from webkitpy.port.headlessdriver import HeadlessDriver

from webkitcorepy import decorators

_log = logging.getLogger(__name__)

class WPEPort(GLibPort):
    port_name = "wpe"
    webdriver_name = "WPEWebDriver"
    supports_localhost_aliases = True

    def __init__(self, *args, **kwargs):
        super(WPEPort, self).__init__(*args, **kwargs)

        if self._display_server == 'xvfb':
            # While not supported by WPE, xvfb is used as the default value in the main scripts
            self._display_server = 'headless'
        self._wpe_legacy_api = self.get_option("wpe_legacy_api")

    def _port_flag_for_scripts(self):
        port = "--wpe"
        if self._wpe_legacy_api:
            port += " --wpe-legacy-api"
        return port

    @memoized
    def _driver_class(self):
        return HeadlessDriver

    def setup_environ_for_server(self, server_name=None):
        environment = super(WPEPort, self).setup_environ_for_server(server_name)
        environment['LIBGL_ALWAYS_SOFTWARE'] = '1'
        # Run WPE tests with Skia CPU (usual configuration on embedded)
        # to help catching issues/crashes <https://webkit.org/b/287632>
        if 'WEBKIT_SKIA_ENABLE_CPU_RENDERING' in environment:
            _log.warning('Ignoring "WEBKIT_SKIA_ENABLE_CPU_RENDERING" variable from environment. Defaulting to value "1".')
        environment['WEBKIT_SKIA_ENABLE_CPU_RENDERING'] = '1'
        self._copy_value_from_environ_if_set(environment, 'XR_RUNTIME_JSON')
        self._copy_value_from_environ_if_set(environment, 'BREAKPAD_MINIDUMP_DIR')
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
        search_paths = []

        if self._wpe_legacy_api:
            search_paths.append('wpe-legacy-api')

        search_paths.append(self.port_name)
        search_paths.append('glib')
        search_paths.append('wk2')
        search_paths.extend(self.get_option("additional_platform_directory", []))
        return search_paths

    def default_baseline_search_path(self, **kwargs):
        return list(map(self._webkit_baseline_path, self._search_paths()))

    def _port_specific_expectations_files(self, **kwargs):
        return list(map(lambda x: self._filesystem.join(self._webkit_baseline_path(x), 'TestExpectations'), reversed(self._search_paths())))

    def configuration_for_upload(self, host=None):
        configuration = super(WPEPort, self).configuration_for_upload(host=host)
        configuration['platform'] = 'WPE'
        return configuration

    def cog_path_to(self, *components):
        return self._build_path('Tools', 'cog-prefix', 'src', 'cog-build', *components)

    def get_browser_path(self, browser_name):
        return self.cog_path_to('launcher', browser_name) if browser_name == 'cog' else self._build_path('bin', browser_name)

    def browser_name(self):
        """Returns the lower case name of the browser to be used (Cog or MiniBrowser)

        Users can select between both with the environment variable WPE_BROWSER
        """
        browser = os.environ.get("WPE_BROWSER", "").lower()
        if browser in ("cog", "minibrowser"):
            return browser

        if browser:
            _log.warning("Unknown browser {}. Defaulting to MiniBrowser".format(browser))

        return "minibrowser"

    def setup_environ_for_minibrowser(self):
        env = super(WPEPort, self).setup_environ_for_minibrowser()

        if self.browser_name() == "cog":
            env['COG_MODULEDIR'] = self.cog_path_to('platform')

        return env

    def setup_environ_for_webdriver(self):
        env = super(WPEPort, self).setup_environ_for_minibrowser()
        # The browser is started from the webdriver process and will inherit
        # the environmnet of the webdriver process. So setup an environmnet
        # that works for any browser (cog, minibrowser)
        env['COG_MODULEDIR'] = self.cog_path_to('platform')
        return env

    def run_minibrowser(self, args):
        miniBrowser = None

        if self.browser_name() == "cog":
            miniBrowser = self.get_browser_path("cog")
            if not self._filesystem.isfile(miniBrowser):
                _log.warning("Cog not found 😢. If you wish to enable it, rebuild with `-DENABLE_COG=ON`. Falling back to MiniBrowser")
                miniBrowser = None
            else:
                print("Using Cog as MiniBrowser")
                has_platform_arg = any((a == "-P" or a.startswith("--platform=") for a in args)) or "COG_PLATFORM_NAME" in os.environ
                if not has_platform_arg:
                    args.insert(0, "--platform=gtk4")

        if not miniBrowser:
            print("Using default MiniBrowser")
            miniBrowser = self.get_browser_path("MiniBrowser")
            if not self._filesystem.isfile(miniBrowser):
                _log.warning("%s not found... Did you run build-webkit?" % miniBrowser)
                return 1
        command = [miniBrowser]
        if os.environ.get("WEBKIT_MINI_BROWSER_PREFIX"):
            command = shlex.split(os.environ["WEBKIT_MINI_BROWSER_PREFIX"]) + command

        env, pass_fds = self.setup_sysprof_for_minibrowser()

        if self._should_use_jhbuild():
            command = self._jhbuild_wrapper + command
        return self._executive.run_command(command + args, cwd=self.webkit_base(), stdout=None, return_stderr=False, decode_output=False, env=env, pass_fds=pass_fds)
