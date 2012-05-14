# Copyright (C) 2011 ProFUSION Embedded Systems. All rights reserved.
# Copyright (C) 2011 Samsung Electronics. All rights reserved.
# Copyright (C) 2012 Intel Corporation
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

"""WebKit Efl implementation of the Port interface."""

import logging
import signal
import subprocess

from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.layout_tests.port.webkit import WebKitPort
from webkitpy.layout_tests.port.pulseaudio_sanitizer import PulseAudioSanitizer


class EflPort(WebKitPort, PulseAudioSanitizer):
    port_name = 'efl'

    def __init__(self, *args, **kwargs):
        WebKitPort.__init__(self, *args, **kwargs)

        self._jhbuild_wrapper_path = self.path_from_webkit_base('Tools', 'efl', 'run-with-jhbuild')

        self.set_option_default('wrapper', self._jhbuild_wrapper_path)

    def _port_flag_for_scripts(self):
        return "--efl"

    def setup_test_run(self):
        self._unload_pulseaudio_module()

    def clean_up_test_run(self):
        self._restore_pulseaudio_module()

    def _generate_all_test_configurations(self):
        return [TestConfiguration(version=self._version, architecture='x86', build_type=build_type) for build_type in self.ALL_BUILD_TYPES]

    def _path_to_driver(self):
        return self._build_path('bin', self.driver_name())

    def _path_to_image_diff(self):
        return self._build_path('bin', 'ImageDiff')

    def _image_diff_command(self, *args, **kwargs):
        return [self._jhbuild_wrapper_path] + super(EflPort, self)._image_diff_command(*args, **kwargs)

    def _path_to_webcore_library(self):
        static_path = self._build_path('WebCore', 'libwebcore_efl.a')
        dyn_path = self._build_path('WebCore', 'libwebcore_efl.so')
        return static_path if self._filesystem.exists(static_path) else dyn_path

    def show_results_html_file(self, results_filename):
        # FIXME: We should find a way to share this implmentation with Gtk,
        # or teach run-launcher how to call run-safari and move this down to WebKitPort.
        run_launcher_args = ["file://%s" % results_filename]
        # FIXME: old-run-webkit-tests also added ["-graphicssystem", "raster", "-style", "windows"]
        # FIXME: old-run-webkit-tests converted results_filename path for cygwin.
        self._run_script("run-launcher", run_launcher_args)
