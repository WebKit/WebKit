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

"""WebKit Haiku implementation of the Port interface."""

import os

from webkitpy.common.system import path
from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.port.base import Port
from webkitpy.port.haikudriver import HaikuDriver
from webkitpy.port.haiku_get_crash_log import HaikuCrashLogGenerator


class HaikuPort(Port):
    port_name = 'haiku'

    def __init__(self, host, port_name, **kwargs):
        super(HaikuPort, self).__init__(host, port_name, **kwargs)

        self.webprocess_cmd_prefix = self.get_option('webprocess_cmd_prefix')
        self._version = "1.4"

    def _port_flag_for_scripts(self):
        return "--haiku"

    def setup_test_run(self, device_class=None):
        super(HaikuPort, self).setup_test_run()

    def setup_environ_for_server(self, server_name=None):
        env = super(HaikuPort, self).setup_environ_for_server(server_name)

        if self.webprocess_cmd_prefix:
            env['WEB_PROCESS_CMD_PREFIX'] = self.webprocess_cmd_prefix

        return env

    def default_timeout_ms(self):
        # Tests run considerably slower under gdb
        # or valgrind.
        if self.get_option('webprocess_cmd_prefix'):
            return 350 * 1000
        return 45 * 1000

    def clean_up_test_run(self):
        super(HaikuPort, self).clean_up_test_run()

    def _generate_all_test_configurations(self):
        return [TestConfiguration(version=self.version_name(), architecture='x86', build_type=build_type) for build_type in self.ALL_BUILD_TYPES]

    def _driver_class(self):
        return HaikuDriver

    def _path_to_driver(self):
        return self._build_path('bin', self.driver_name())

    def _path_to_image_diff(self):
        return self._build_path('bin', 'ImageDiff')

    def _image_diff_command(self, *args, **kwargs):
        return super(HaikuPort, self)._image_diff_command(*args, **kwargs)

    def _path_to_webcore_library(self):
        static_path = self._build_path('lib', 'libWebCore.a')
        dyn_path = self._build_path('lib', 'libWebCore.so')
        return static_path if self._filesystem.exists(static_path) else dyn_path

    def _search_paths(self):
        search_paths = []
        if self.get_option('webkit_test_runner'):
            search_paths.append(self.port_name + '-wk2')
            search_paths.append('wk2')
        else:
            search_paths.append(self.port_name + '-wk1')
        search_paths.append(self.port_name)
        return search_paths

    def default_baseline_search_path(self, **kwargs):
        return map(self._webkit_baseline_path, self._search_paths())

    def _port_specific_expectations_files(self, **kwargs):
        return [self._filesystem.join(self._webkit_baseline_path(p), 'TestExpectations') for p in reversed(self._search_paths())]

    def show_results_html_file(self, results_filename):
        self._run_script("run-minibrowser", [path.abspath_to_uri(self.host.platform, results_filename)])

    def _uses_apache(self):
        return False

    def _path_to_lighttpd(self):
        return "/system/bin/lighttpd"

    def _path_to_lighttpd_modules(self):
        if os.path.exists("/system/lib/lighttpd"):
            return "/system/lib/lighttpd"
        else:
            return "/system/lib/x86/lighttpd"

    def _path_to_lighttpd_php(self):
        return "/system/bin/php-cgi"

    def _path_to_lighttpd_env(self):
        return "/bin/env"

    def check_sys_deps(self):
        return super(HaikuPort, self).check_sys_deps() and self._driver_class().check_driver(self)

    def build_webkit_command(self, build_style=None):
        command = super(HaikuPort, self).build_webkit_command(build_style)
        command.extend(["--haiku", "--update-haiku"])
        if self.get_option('webkit_test_runner'):
            command.append("--no-webkit1")
        else:
            command.append("--no-webkit2")
        command.append(super(HaikuPort, self).make_args())
        return command

    def _get_crash_log(self, name, pid, stdout, stderr, newer_than, target_host=None):
        return HaikuCrashLogGenerator(name, pid, newer_than, self._filesystem, self._path_to_driver).generate_crash_log(stdout, stderr)
