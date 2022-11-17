# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2013 Samsung Electronics.  All rights reserved.
# Copyright (C) 2017 Igalia S.L. All rights reserved.
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
import logging
import shlex

import webkitpy
from webkitpy.common.system import path
from webkitpy.common.memoized import memoized
from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.port.glib import GLibPort
from webkitpy.port.xvfbdriver import XvfbDriver
from webkitpy.port.westondriver import WestonDriver
from webkitpy.port.xorgdriver import XorgDriver
from webkitpy.port.waylanddriver import WaylandDriver

from webkitcorepy import decorators

_log = logging.getLogger(__name__)


class GtkPort(GLibPort):
    port_name = "gtk"
    supports_localhost_aliases = True

    def _port_flag_for_scripts(self):
        return "--gtk"

    @memoized
    def _driver_class(self):
        if self._display_server == "weston":
            return WestonDriver
        if self._display_server == "wayland":
            return WaylandDriver
        if self._display_server == "xorg":
            return XorgDriver
        return XvfbDriver

    def driver_stop_timeout(self):
        if self.get_option("leaks"):
            # Wait the default timeout time before killing the process in driver.stop().
            return self.default_timeout_ms()
        return super(GtkPort, self).driver_stop_timeout()

    def setup_environ_for_server(self, server_name=None):
        environment = super(GtkPort, self).setup_environ_for_server(server_name)
        environment['LIBOVERLAY_SCROLLBAR'] = '0'

        # Configure the software libgl renderer if jhbuild ready and we test inside a virtualized window system
        if self._driver_class() in [XvfbDriver, WestonDriver] and (self._should_use_jhbuild() or self._is_flatpak()):
            if self._should_use_jhbuild():
                llvmpipe_libgl_path = self.host.executive.run_command(self._jhbuild_wrapper + ['printenv', 'LLVMPIPE_LIBGL_PATH'],
                                                                    ignore_errors=True).strip()
                dri_libgl_path = os.path.join(llvmpipe_libgl_path, "dri")
            else:  # in flatpak
                llvmpipe_libgl_path = "/usr/lib/x86_64-linux-gnu/"
                dri_libgl_path = os.path.join(llvmpipe_libgl_path, "GL", "lib", "dri")

            if os.path.exists(os.path.join(llvmpipe_libgl_path, "libGL.so")) and os.path.exists(os.path.join(dri_libgl_path, "swrast_dri.so")):
                # Make sure va-api support gets disabled because it's incompatible with Mesa's softGL driver.
                environment['LIBVA_DRIVER_NAME'] = "null"
                # Force the Gallium llvmpipe software rasterizer
                environment['LIBGL_ALWAYS_SOFTWARE'] = "1"
                environment['LIBGL_DRIVERS_PATH'] = dri_libgl_path
            else:
                _log.warning("Can't find Gallium llvmpipe driver. Try to run update-webkitgtk-libs or update-webkit-flatpak")
        return environment

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

    def _path_to_webcore_library(self):
        gtk_library_names = [
            "libwebkitgtk-1.0.so",
            "libwebkitgtk-3.0.so",
            "libwebkit2gtk-1.0.so",
        ]

        for library in gtk_library_names:
            full_library = self._built_libraries_path(library)
            if self._filesystem.isfile(full_library):
                return full_library
        return None

    def _search_paths(self):
        search_paths = []

        if self._is_gtk4_build():
            search_paths.append(self.port_name + "4")

        if self._driver_class() in [WaylandDriver, WestonDriver]:
            search_paths.append(self.port_name + "-wayland")

        search_paths.append(self.port_name)
        search_paths.append('glib')
        search_paths.append('wk2')
        search_paths.extend(self.get_option("additional_platform_directory", []))
        return search_paths

    def default_baseline_search_path(self, **kwargs):
        return list(map(self._webkit_baseline_path, self._search_paths()))

    def _port_specific_expectations_files(self, **kwargs):
        return [self._filesystem.join(self._webkit_baseline_path(p), 'TestExpectations') for p in reversed(self._search_paths())]

    def print_leaks_summary(self):
        if not self.get_option('leaks'):
            return
        # FIXME: This is a hack, but we don't have a better way to get this information from the workers yet
        # because we're in the manager process.
        leaks_files = self._leakdetector.leaks_files_in_results_directory()
        if not leaks_files:
            return
        self._leakdetector.parse_and_print_leaks_detail(leaks_files)

    def show_results_html_file(self, results_filename):
        self.run_minibrowser([path.abspath_to_uri(self.host.platform, results_filename)])

    def check_sys_deps(self):
        return super(GtkPort, self).check_sys_deps() and self._driver_class().check_driver(self)

    def test_expectations_file_position(self):
        # GTK port baseline search path is gtk -> glib -> wk2 -> generic (as gtk-wk2 and gtk baselines are merged), so port test expectations file is at third to last position.
        return 3

    def build_webkit_command(self, build_style=None):
        command = super(GtkPort, self).build_webkit_command(build_style)
        command.extend(["--gtk", "--update-gtk"])
        command.append(super(GtkPort, self).make_args())
        return command

    def run_webkit_tests_command(self):
        command = super(GtkPort, self).run_webkit_tests_command()
        command.append("--gtk")
        return command

    def configuration_for_upload(self, host=None):
        configuration = super(GtkPort, self).configuration_for_upload(host=host)
        configuration['platform'] = 'GTK'
        configuration['version_name'] = self._display_server.capitalize() if self._display_server else 'Xvfb'
        return configuration

    def run_minibrowser(self, args):
        miniBrowser = self._build_path('bin', 'MiniBrowser')
        if not self._filesystem.isfile(miniBrowser):
            print("%s not found... Did you run build-webkit?" % miniBrowser)
            return 1
        command = [miniBrowser]
        if os.environ.get("WEBKIT_MINI_BROWSER_PREFIX"):
            command = shlex.split(os.environ["WEBKIT_MINI_BROWSER_PREFIX"]) + command

        if self._should_use_jhbuild():
            command = self._jhbuild_wrapper + command
        return self._executive.run_command(command + args, cwd=self.webkit_base(), stdout=None, return_stderr=False, decode_output=False, env=self.setup_environ_for_minibrowser())

    @memoized
    def _is_gtk4_build(self):
        try:
            libdir = self._build_path('lib')
            candidates = self._filesystem.glob(os.path.join(libdir, 'libwebkit2gtk-*.so'))
            if not candidates:
                return False
            if len(candidates) > 1:
                _log.warning("Multiple WebKit2GTK libraries found. Skipping GTK4 detection.")
                return False
            return os.path.basename(candidates[0]) == 'libwebkit2gtk-5.0.so'

        except (webkitpy.common.system.executive.ScriptError, IOError, OSError):
            return False
        return False
