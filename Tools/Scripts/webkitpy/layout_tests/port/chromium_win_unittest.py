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

import os
import unittest

from webkitpy.common.system import outputcapture
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.layout_tests.port import chromium_win
from webkitpy.layout_tests.port import port_testcase


class ChromiumWinTest(port_testcase.PortTestCase):
    class RegisterCygwinOption(object):
        def __init__(self):
            self.register_cygwin = True
            self.results_directory = '/'

    port_maker = chromium_win.ChromiumWinPort
    os_name = 'win'
    os_version = 'xp'

    def test_uses_apache(self):
        self.assertFalse(self.make_port()._uses_apache())

    def test_setup_environ_for_server(self):
        port = self.make_port()
        port._executive = MockExecutive(should_log=True)
        output = outputcapture.OutputCapture()
        # FIXME: This test should not use the real os.environ
        orig_environ = os.environ.copy()
        env = output.assert_outputs(self, port.setup_environ_for_server)
        self.assertEqual(orig_environ["PATH"], os.environ["PATH"])
        self.assertNotEqual(env["PATH"], os.environ["PATH"])

    def test_setup_environ_for_server_cygpath(self):
        port = self.make_port()
        env = port.setup_environ_for_server(port.driver_name())
        self.assertEquals(env['CYGWIN_PATH'], '/mock-checkout/Source/WebKit/chromium/third_party/cygwin/bin')

    def test_setup_environ_for_server_register_cygwin(self):
        port = self.make_port(options=ChromiumWinTest.RegisterCygwinOption())
        port._executive = MockExecutive(should_log=True)
        expected_stderr = "MOCK run_command: ['/mock-checkout/Source/WebKit/chromium/third_party/cygwin/setup_mount.bat'], cwd=None\n"
        output = outputcapture.OutputCapture()
        output.assert_outputs(self, port.setup_environ_for_server, expected_stderr=expected_stderr)

    def assert_name(self, port_name, os_version_string, expected):
        port = self.make_port(port_name=port_name, os_version=os_version_string)
        self.assertEquals(expected, port.name())

    def test_versions(self):
        port = self.make_port()
        self.assertTrue(port.name() in ('chromium-win-xp', 'chromium-win-vista', 'chromium-win-win7'))

        self.assert_name(None, 'xp', 'chromium-win-xp')
        self.assert_name('chromium-win', 'xp', 'chromium-win-xp')
        self.assert_name('chromium-win-xp', 'xp', 'chromium-win-xp')
        self.assert_name('chromium-win-xp', 'vista', 'chromium-win-xp')
        self.assert_name('chromium-win-xp', '7sp0', 'chromium-win-xp')

        self.assert_name(None, 'vista', 'chromium-win-vista')
        self.assert_name('chromium-win', 'vista', 'chromium-win-vista')
        self.assert_name('chromium-win-vista', 'xp', 'chromium-win-vista')
        self.assert_name('chromium-win-vista', 'vista', 'chromium-win-vista')
        self.assert_name('chromium-win-vista', '7sp0', 'chromium-win-vista')

        self.assert_name(None, '7sp0', 'chromium-win-win7')
        self.assert_name('chromium-win', '7sp0', 'chromium-win-win7')
        self.assert_name('chromium-win-win7', 'xp', 'chromium-win-win7')
        self.assert_name('chromium-win-win7', 'vista', 'chromium-win-win7')
        self.assert_name('chromium-win-win7', '7sp0', 'chromium-win-win7')

        self.assertRaises(AssertionError, self.assert_name, None, 'w2k', 'chromium-win-xp')

    def test_baseline_path(self):
        port = self.make_port(port_name='chromium-win-xp')
        self.assertEquals(port.baseline_path(), port._webkit_baseline_path('chromium-win-xp'))

        port = self.make_port(port_name='chromium-win-vista')
        self.assertEquals(port.baseline_path(), port._webkit_baseline_path('chromium-win-vista'))

        port = self.make_port(port_name='chromium-win-win7')
        self.assertEquals(port.baseline_path(), port._webkit_baseline_path('chromium-win'))

    def test_build_path(self):
        port = self.make_port()
        port._filesystem.files = {
            '/mock-checkout/Source/WebKit/chromium/build/Release/DumpRenderTree.exe': 'exe',
        }
        self.assertEquals(
            '/mock-checkout/Source/WebKit/chromium/build/Release/DumpRenderTree.exe',
            port._path_to_driver('Release'))
        self.assertEquals(
            '/mock-checkout/Source/WebKit/chromium/build/Debug/DumpRenderTree.exe',
            port._path_to_driver('Debug'))

    def test_operating_system(self):
        self.assertEqual('win', self.make_port().operating_system())
