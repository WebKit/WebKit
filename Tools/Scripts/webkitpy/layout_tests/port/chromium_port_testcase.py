# Copyright (C) 2010 Google Inc. All rights reserved.
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
#    * Neither the name of Google Inc. nor the names of its
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

import unittest

from webkitpy.common.system import logtesting
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.tool.mocktool import MockOptions

import chromium_android
import chromium_linux
import chromium_mac
import chromium_win

from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.layout_tests.port import port_testcase


class ChromiumPortTestCase(port_testcase.PortTestCase):

    def test_check_build(self):
        port = self.make_port()
        port.check_build(needs_http=True)

    def test_default_timeout_ms(self):
        self.assertEquals(self.make_port(options=MockOptions(configuration='Release')).default_timeout_ms(), 6000)
        self.assertEquals(self.make_port(options=MockOptions(configuration='Debug')).default_timeout_ms(), 12000)

    def test_default_pixel_tests(self):
        self.assertEquals(self.make_port().default_pixel_tests(), True)

    def test_missing_symbol_to_skipped_tests(self):
        # Test that we get the chromium skips and not the webkit default skips
        port = self.make_port()
        skip_dict = port._missing_symbol_to_skipped_tests()
        self.assertTrue('ff_mp3_decoder' in skip_dict)
        self.assertFalse('WebGLShader' in skip_dict)

    def test_all_test_configurations(self):
        """Validate the complete set of configurations this port knows about."""
        port = self.make_port()
        self.assertEquals(set(port.all_test_configurations()), set([
            TestConfiguration('icecreamsandwich', 'x86', 'debug'),
            TestConfiguration('icecreamsandwich', 'x86', 'release'),
            TestConfiguration('snowleopard', 'x86', 'debug'),
            TestConfiguration('snowleopard', 'x86', 'release'),
            TestConfiguration('lion', 'x86', 'debug'),
            TestConfiguration('lion', 'x86', 'release'),
            TestConfiguration('xp', 'x86', 'debug'),
            TestConfiguration('xp', 'x86', 'release'),
            TestConfiguration('win7', 'x86', 'debug'),
            TestConfiguration('win7', 'x86', 'release'),
            TestConfiguration('lucid', 'x86', 'debug'),
            TestConfiguration('lucid', 'x86', 'release'),
            TestConfiguration('lucid', 'x86_64', 'debug'),
            TestConfiguration('lucid', 'x86_64', 'release'),
        ]))

    class TestMacPort(chromium_mac.ChromiumMacPort):
        def __init__(self, options=None):
            options = options or MockOptions()
            chromium_mac.ChromiumMacPort.__init__(self, MockSystemHost(os_name='mac', os_version='leopard'), 'chromium-mac-leopard', options=options)

        def default_configuration(self):
            self.default_configuration_called = True
            return 'default'

    class TestAndroidPort(chromium_android.ChromiumAndroidPort):
        def __init__(self, options=None):
            options = options or MockOptions()
            chromium_win.ChromiumAndroidPort.__init__(self, MockSystemHost(os_name='android', os_version='icecreamsandwich'), 'chromium-android', options=options)

        def default_configuration(self):
            self.default_configuration_called = True
            return 'default'

    class TestLinuxPort(chromium_linux.ChromiumLinuxPort):
        def __init__(self, options=None):
            options = options or MockOptions()
            chromium_linux.ChromiumLinuxPort.__init__(self, MockSystemHost(os_name='linux', os_version='lucid'), 'chromium-linux-x86', options=options)

        def default_configuration(self):
            self.default_configuration_called = True
            return 'default'

    class TestWinPort(chromium_win.ChromiumWinPort):
        def __init__(self, options=None):
            options = options or MockOptions()
            chromium_win.ChromiumWinPort.__init__(self, MockSystemHost(os_name='win', os_version='xp'), 'chromium-win-xp', options=options)

        def default_configuration(self):
            self.default_configuration_called = True
            return 'default'

    def test_default_configuration(self):
        mock_options = MockOptions()
        port = ChromiumPortTestCase.TestLinuxPort(options=mock_options)
        self.assertEquals(mock_options.configuration, 'default')
        self.assertTrue(port.default_configuration_called)

        mock_options = MockOptions(configuration=None)
        port = ChromiumPortTestCase.TestLinuxPort(mock_options)
        self.assertEquals(mock_options.configuration, 'default')
        self.assertTrue(port.default_configuration_called)

    def test_diff_image(self):
        class TestPort(ChromiumPortTestCase.TestLinuxPort):
            def _path_to_image_diff(self):
                return "/path/to/image_diff"

        port = ChromiumPortTestCase.TestLinuxPort()
        mock_image_diff = "MOCK Image Diff"

        def mock_run_command(args):
            port._filesystem.write_binary_file(args[4], mock_image_diff)
            return 1

        # Images are different.
        port._executive = MockExecutive2(run_command_fn=mock_run_command)
        self.assertEquals(mock_image_diff, port.diff_image("EXPECTED", "ACTUAL")[0])

        # Images are the same.
        port._executive = MockExecutive2(exit_code=0)
        self.assertEquals(None, port.diff_image("EXPECTED", "ACTUAL")[0])

        # There was some error running image_diff.
        port._executive = MockExecutive2(exit_code=2)
        exception_raised = False
        try:
            port.diff_image("EXPECTED", "ACTUAL")
        except ValueError, e:
            exception_raised = True
        self.assertFalse(exception_raised)

    def test_diff_image_crashed(self):
        port = ChromiumPortTestCase.TestLinuxPort()
        port._executive = MockExecutive2(exit_code=2)
        self.assertEquals(port.diff_image("EXPECTED", "ACTUAL"), (None, 0, 'image diff returned an exit code of 2'))

    def test_expectations_files(self):
        port = self.make_port()
        port.port_name = 'chromium'

        expectations_path = port.path_to_test_expectations_file()
        chromium_overrides_path = port.path_from_chromium_base(
            'webkit', 'tools', 'layout_tests', 'test_expectations.txt')
        skia_overrides_path = port.path_from_chromium_base(
            'skia', 'skia_test_expectations.txt')

        port._filesystem.write_text_file(skia_overrides_path, 'dummay text')

        port._options.builder_name = 'DUMMY_BUILDER_NAME'
        self.assertEquals(port.expectations_files(), [expectations_path, skia_overrides_path, chromium_overrides_path])

        port._options.builder_name = 'builder (deps)'
        self.assertEquals(port.expectations_files(), [expectations_path, skia_overrides_path, chromium_overrides_path])

        # A builder which does NOT observe the Chromium test_expectations,
        # but still observes the Skia test_expectations...
        port._options.builder_name = 'builder'
        self.assertEquals(port.expectations_files(), [expectations_path, skia_overrides_path])

    def test_expectations_ordering(self):
        # since we don't implement self.port_name in ChromiumPort.
        pass


class ChromiumPortLoggingTest(logtesting.LoggingTestCase):
    def test_check_sys_deps(self):
        port = ChromiumPortTestCase.TestLinuxPort()

        # Success
        port._executive = MockExecutive2(exit_code=0)
        self.assertTrue(port.check_sys_deps(needs_http=False))

        # Failure
        port._executive = MockExecutive2(exit_code=1,
            output='testing output failure')
        self.assertFalse(port.check_sys_deps(needs_http=False))
        self.assertLog([
            'ERROR: System dependencies check failed.\n',
            'ERROR: To override, invoke with --nocheck-sys-deps\n',
            'ERROR: \n',
            'ERROR: testing output failure\n'])


if __name__ == '__main__':
    unittest.main()
