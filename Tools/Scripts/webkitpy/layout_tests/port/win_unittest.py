# Copyright (C) 2011 Google Inc. All rights reserved.
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

import StringIO
import sys
import unittest

from webkitpy.common.system.executive import ScriptError
from webkitpy.common.system.executive_mock import MockExecutive, MockExecutive2
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.layout_tests.port import port_testcase
from webkitpy.layout_tests.port.win import WinPort
from webkitpy.tool.mocktool import MockOptions


class WinPortTest(port_testcase.PortTestCase):
    port_maker = WinPort

    def test_show_results_html_file(self):
        port = self.make_port()
        port._executive = MockExecutive(should_log=True)
        expected_stderr = "MOCK: user.open_url: test.html\n"
        OutputCapture().assert_outputs(self, port.show_results_html_file, ["test.html"], expected_stderr=expected_stderr)

    def test_detect_version(self):
        port = self.make_port()

        def mock_run_command(cmd):
            self.assertEquals(cmd, ['cmd', '/c', 'ver'])
            return "6.1.7600"

        port._executive = MockExecutive2(run_command_fn=mock_run_command)
        self.assertEquals(port._detect_version(run_on_non_windows_platforms=True), '7sp0')

        def mock_run_command(cmd):
            raise ScriptError('Failure')

        port._executive = MockExecutive2(run_command_fn=mock_run_command)
        # Failures log to the python error log, but we have no easy way to capture/test that.
        self.assertEquals(port._detect_version(run_on_non_windows_platforms=True), None)

    def _assert_search_path(self, expected_search_paths, version, use_webkit2=False):
        port = WinPort(os_version_string=version,
            options=MockOptions(webkit_test_runner=use_webkit2),
            host=MockHost())
        absolute_search_paths = map(port._webkit_baseline_path, expected_search_paths)
        self.assertEquals(port.baseline_search_path(), absolute_search_paths)

    def test_baseline_search_path(self):
        self._assert_search_path(['win-xp', 'win-vista', 'win-7sp0', 'win', 'mac-lion', 'mac'], 'xp')
        self._assert_search_path(['win-vista', 'win-7sp0', 'win', 'mac-lion', 'mac'], 'vista')
        self._assert_search_path(['win-7sp0', 'win', 'mac-lion', 'mac'], '7sp0')
        self._assert_search_path(['win', 'mac-lion', 'mac'], 'bogus')

        self._assert_search_path(['win-wk2', 'win-xp', 'win-vista', 'win-7sp0', 'win', 'mac-wk2', 'mac-lion', 'mac'], 'xp', use_webkit2=True)
        self._assert_search_path(['win-wk2', 'win-vista', 'win-7sp0', 'win', 'mac-wk2', 'mac-lion', 'mac'], 'vista', use_webkit2=True)
        self._assert_search_path(['win-wk2', 'win-7sp0', 'win', 'mac-wk2', 'mac-lion', 'mac'], '7sp0', use_webkit2=True)
        self._assert_search_path(['win-wk2', 'win', 'mac-wk2', 'mac-lion', 'mac'], 'bogus', use_webkit2=True)

    def _assert_version(self, port_name, expected_version):
        port = WinPort(port_name=port_name, host=MockHost())
        self.assertEquals(port.version(), expected_version)

    def test_versions(self):
        self._assert_version('win-xp', 'xp')
        self._assert_version('win-vista', 'vista')
        self._assert_version('win-7sp0', '7sp0')

    def test_compare_text(self):
        expected = "EDITING DELEGATE: webViewDidChangeSelection:WebViewDidChangeSelectionNotification\nfoo\nEDITING DELEGATE: webViewDidChangeSelection:WebViewDidChangeSelectionNotification\n"
        port = self.make_port()
        self.assertFalse(port.compare_text(expected, "foo\n"))
        self.assertTrue(port.compare_text(expected, "foo"))
        self.assertTrue(port.compare_text(expected, "bar"))

        # This hack doesn't exist in WK2.
        port._options = MockOptions(webkit_test_runner=True)
        self.assertTrue(port.compare_text(expected, "foo\n"))

    def test_operating_system(self):
        self.assertEqual('win', self.make_port().operating_system())
