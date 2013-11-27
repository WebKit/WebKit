# Copyright (C) 2011 ProFUSION Embedded Systems. All rights reserved.
# Copyright (C) 2011 Samsung Electronics. All rights reserved.
# Copyright (C) 2012 Intel Corporation
# Copyright (C) 2012, 2013 Nokia Corporation and/or its subsidiary(-ies).
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

"""WebKit Nix implementation of the Port interface."""

import os

from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.port.base import Port
from webkitpy.port.pulseaudio_sanitizer import PulseAudioSanitizer


class NixPort(Port):
    port_name = 'nix'

    def _wk2_port_name(self):
        return 'nix'

    @classmethod
    def determine_full_port_name(cls, host, options, port_name):
        """Determine the port name based on host and options values."""
        # Currently the only "port" instance supported by Nix is "nix". Reimplementing
        # this method avoids changing port name to nix-wk2, which is the default
        # behavior in base.py.
        return port_name

    def __init__(self, *args, **kwargs):
        super(NixPort, self).__init__(*args, **kwargs)

        self._jhbuild_wrapper_path = [self.path_from_webkit_base('Tools', 'jhbuild', 'jhbuild-wrapper'), '--nix', 'run']

        self.set_option_default('wrapper', ' '.join(self._jhbuild_wrapper_path))
        self.set_option_default('webkit_test_runner', True)
        self.webprocess_cmd_prefix = self.get_option('webprocess_cmd_prefix')

        self._pulseaudio_sanitizer = PulseAudioSanitizer()

    def _port_flag_for_scripts(self):
        return "--nix"

    def setup_test_run(self):
        self._pulseaudio_sanitizer.unload_pulseaudio_module()

    def setup_environ_for_server(self, server_name=None):
        env = super(NixPort, self).setup_environ_for_server(server_name)
        # If DISPLAY environment variable is unset in the system
        # e.g. on build bot, remove DISPLAY variable from the dictionary
        if not 'DISPLAY' in os.environ:
            del env['DISPLAY']
        env['TEST_RUNNER_INJECTED_BUNDLE_FILENAME'] = self._build_path('lib', 'libTestRunnerInjectedBundle.so')
        env['TEST_RUNNER_PLUGIN_PATH'] = self._build_path('lib')
        if self.webprocess_cmd_prefix:
            env['WEB_PROCESS_CMD_PREFIX'] = self.webprocess_cmd_prefix

        return env

    def default_timeout_ms(self):
        # Tests run considerably slower under gdb
        # or valgrind.
        if self.get_option('webprocess_cmd_prefix'):
            return 350 * 1000
        return super(NixPort, self).default_timeout_ms()

    def clean_up_test_run(self):
        super(NixPort, self).clean_up_test_run()
        self._pulseaudio_sanitizer.restore_pulseaudio_module()

    def _generate_all_test_configurations(self):
        return [TestConfiguration(version=self._version, architecture='x86', build_type=build_type) for build_type in self.ALL_BUILD_TYPES]

    def _path_to_driver(self):
        return self._build_path('bin', self.driver_name())

    def _path_to_image_diff(self):
        return self._build_path('bin', 'ImageDiff')

    def _image_diff_command(self, *args, **kwargs):
        return self._jhbuild_wrapper_path + super(NixPort, self)._image_diff_command(*args, **kwargs)

    def _search_paths(self):
        return [self.port_name]

    def show_results_html_file(self, results_filename):
        run_launcher_args = ["file://%s" % results_filename]
        self._run_script("run-launcher", run_launcher_args)

    def _port_specific_expectations_files(self):
        paths = self._search_paths()
        if self.get_option('webkit_test_runner'):
            paths.append('wk2')
        return list(([self._filesystem.join(self._webkit_baseline_path(p), 'TestExpectations') for p in paths]))

    def default_baseline_search_path(self):
        return map(self._webkit_baseline_path, ['nix', 'wk2'])
