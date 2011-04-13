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
import sys
import unittest

from webkitpy.common.system import outputcapture
from webkitpy.tool import mocktool

from webkitpy.layout_tests.port import chromium_win
from webkitpy.layout_tests.port import port_testcase


class ChromiumWinTest(port_testcase.PortTestCase):
    class RegisterCygwinOption(object):
        def __init__(self):
            self.register_cygwin = True
            self.results_directory = '/'

    def setUp(self):
        self.orig_platform = sys.platform

    def tearDown(self):
        sys.platform = self.orig_platform
        self._port = None

    def port_maker(self, platform):
        if platform not in ('cygwin', 'win32'):
            return None
        return chromium_win.ChromiumWinPort

    def _mock_path_from_chromium_base(self, *comps):
        return self._port._filesystem.join("/chromium/src", *comps)

    def test_setup_environ_for_server(self):
        port = self.make_port()
        if not port:
            return

        port._executive = mocktool.MockExecutive(should_log=True)
        self._port = port
        port.path_from_chromium_base = self._mock_path_from_chromium_base
        output = outputcapture.OutputCapture()
        orig_environ = os.environ.copy()
        env = output.assert_outputs(self, port.setup_environ_for_server)
        self.assertEqual(orig_environ["PATH"], os.environ["PATH"])
        self.assertNotEqual(env["PATH"], os.environ["PATH"])

    def test_setup_environ_for_server_register_cygwin(self):
        port = self.make_port(options=ChromiumWinTest.RegisterCygwinOption())
        if not port:
            return

        port._executive = mocktool.MockExecutive(should_log=True)
        port.path_from_chromium_base = self._mock_path_from_chromium_base
        self._port = port
        setup_mount = self._mock_path_from_chromium_base("third_party",
                                                        "cygwin",
                                                        "setup_mount.bat")
        expected_stderr = "MOCK run_command: %s\n" % [setup_mount]
        output = outputcapture.OutputCapture()
        output.assert_outputs(self, port.setup_environ_for_server,
                              expected_stderr=expected_stderr)

    def assert_name(self, port_name, windows_version, expected):
        port = chromium_win.ChromiumWinPort(port_name=port_name,
                                            windows_version=windows_version)
        self.assertEquals(expected, port.name())

    def test_versions(self):
        port = chromium_win.ChromiumWinPort()
        self.assertTrue(port.name() in ('chromium-win-xp', 'chromium-win-vista', 'chromium-win-win7'))

        self.assert_name(None, (5, 1), 'chromium-win-xp')
        self.assert_name('chromium-win', (5, 1), 'chromium-win-xp')
        self.assert_name('chromium-win-xp', (5, 1), 'chromium-win-xp')
        self.assert_name('chromium-win-xp', (6, 0), 'chromium-win-xp')
        self.assert_name('chromium-win-xp', (6, 1), 'chromium-win-xp')

        self.assert_name(None, (6, 0), 'chromium-win-vista')
        self.assert_name('chromium-win', (6, 0), 'chromium-win-vista')
        self.assert_name('chromium-win-vista', (5, 1), 'chromium-win-vista')
        self.assert_name('chromium-win-vista', (6, 0), 'chromium-win-vista')
        self.assert_name('chromium-win-vista', (6, 1), 'chromium-win-vista')

        self.assert_name(None, (6, 1), 'chromium-win-win7')
        self.assert_name('chromium-win', (6, 1), 'chromium-win-win7')
        self.assert_name('chromium-win-win7', (5, 1), 'chromium-win-win7')
        self.assert_name('chromium-win-win7', (6, 0), 'chromium-win-win7')
        self.assert_name('chromium-win-win7', (6, 1), 'chromium-win-win7')

        self.assertRaises(KeyError, self.assert_name, None, (4, 0), 'chromium-win-xp')
        self.assertRaises(KeyError, self.assert_name, None, (5, 0), 'chromium-win-xp')
        self.assertRaises(KeyError, self.assert_name, None, (5, 2), 'chromium-win-xp')
        self.assertRaises(KeyError, self.assert_name, None, (7, 1), 'chromium-win-xp')

    def test_baseline_path(self):
        port = chromium_win.ChromiumWinPort(port_name='chromium-win-xp')
        self.assertEquals(port.baseline_path(), port._webkit_baseline_path('chromium-win-xp'))

        port = chromium_win.ChromiumWinPort(port_name='chromium-win-vista')
        self.assertEquals(port.baseline_path(), port._webkit_baseline_path('chromium-win-vista'))

        port = chromium_win.ChromiumWinPort(port_name='chromium-win-win7')
        self.assertEquals(port.baseline_path(), port._webkit_baseline_path('chromium-win'))



if __name__ == '__main__':
    unittest.main()
