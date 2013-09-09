# Copyright (C) 2011 ProFUSION Embedded Systems. All rights reserved.
# Copyright (C) 2011 Samsung Electronics. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
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

import unittest2 as unittest

from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.port.efl import EflPort
from webkitpy.port.pulseaudio_sanitizer_mock import PulseAudioSanitizerMock
from webkitpy.port import port_testcase
from webkitpy.port.base import Port
from webkitpy.tool.mocktool import MockOptions


class EflPortTest(port_testcase.PortTestCase):
    port_name = 'efl'
    port_maker = EflPort

    # Additionally mocks out the PulseAudioSanitizer methods.
    def make_port(self, host=None, port_name=None, options=None, os_name=None, os_version=None, **kwargs):
        port = super(EflPortTest, self).make_port(host, port_name, options, os_name, os_version, **kwargs)
        port._pulseaudio_sanitizer = PulseAudioSanitizerMock()
        return port

    def test_show_results_html_file(self):
        port = self.make_port()
        port._executive = MockExecutive(should_log=True)
        expected_logs = "MOCK run_command: ['Tools/Scripts/run-launcher', '--release', '--efl', 'file://test.html'], cwd=/mock-checkout\n"
        OutputCapture().assert_outputs(self, port.show_results_html_file, ["test.html"], expected_logs=expected_logs)

    def test_commands(self):
        port = self.make_port(port_name="efl")
        self.assertEqual(port.tooling_flag(), "--port=efl")
        self.assertEqual(port.update_webkit_command(), Port.script_shell_command("update-webkit"))
        self.assertEqual(port.check_webkit_style_command(), Port.script_shell_command("check-webkit-style"))
        self.assertEqual(port.prepare_changelog_command(), Port.script_shell_command("prepare-ChangeLog"))
        self.assertEqual(port.build_webkit_command(), Port.script_shell_command("build-webkit") + ["--efl", "--update-efl", "--no-webkit2", port.make_args()])
        self.assertEqual(port.run_javascriptcore_tests_command(), Port.script_shell_command("run-javascriptcore-tests"))
        self.assertEqual(port.run_webkit_unit_tests_command(), None)
        self.assertEqual(port.run_webkit_tests_command(), Port.script_shell_command("run-webkit-tests"))
        self.assertEqual(port.run_python_unittests_command(), Port.script_shell_command("test-webkitpy"))
        self.assertEqual(port.run_perl_unittests_command(), Port.script_shell_command("test-webkitperl"))
        self.assertEqual(port.run_bindings_tests_command(), Port.script_shell_command("run-bindings-tests"))

        port = self.make_port(port_name="efl", options=MockOptions(webkit_test_runner=True))
        self.assertEqual(port.tooling_flag(), "--port=efl-wk2")
        self.assertEqual(port.update_webkit_command(), Port.script_shell_command("update-webkit"))
        self.assertEqual(port.check_webkit_style_command(), Port.script_shell_command("check-webkit-style"))
        self.assertEqual(port.prepare_changelog_command(), Port.script_shell_command("prepare-ChangeLog"))
        self.assertEqual(port.build_webkit_command(), Port.script_shell_command("build-webkit") + ["--efl", "--update-efl", "--no-webkit1", port.make_args()])
        self.assertEqual(port.run_javascriptcore_tests_command(), Port.script_shell_command("run-javascriptcore-tests"))
        self.assertEqual(port.run_webkit_unit_tests_command(), None)
        self.assertEqual(port.run_webkit_tests_command(), Port.script_shell_command("run-webkit-tests"))
        self.assertEqual(port.run_python_unittests_command(), Port.script_shell_command("test-webkitpy"))
        self.assertEqual(port.run_perl_unittests_command(), Port.script_shell_command("test-webkitperl"))
        self.assertEqual(port.run_bindings_tests_command(), Port.script_shell_command("run-bindings-tests"))
