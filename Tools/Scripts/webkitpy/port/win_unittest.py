# Copyright (C) 2011 Google Inc. All rights reserved.
# Copyright (C) 2020 Apple Inc. All rights reserved.
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

import logging

from webkitcorepy import Version, OutputCapture

from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.common.version_name_map import PUBLIC_TABLE, VersionNameMap
from webkitpy.port import port_testcase
from webkitpy.port.win import WinPort
from webkitpy.tool.mocktool import MockOptions


class WinPortTest(port_testcase.PortTestCase):
    os_name = 'win'
    os_version = Version.from_name('XP')
    port_name = 'win-xp'
    port_maker = WinPort

    def _assert_search_path(self, expected_search_paths, version, use_webkit2=False):
        port = self.make_port(port_name='win', os_version=version, options=MockOptions(webkit_test_runner=use_webkit2))
        platform_dir = port.host.filesystem.dirname(port._webkit_baseline_path("xxx"))
        relative_search_paths = [port.host.filesystem.relpath(p, platform_dir) for p in port.baseline_search_path()]
        self.assertEqual(relative_search_paths, expected_search_paths)

    def test_baseline_search_path(self):
        self._assert_search_path(['win-xp-wk1', 'win-xp', 'win-vista-wk1', 'win-vista', 'win-7sp0-wk1', 'win-7sp0', 'win-8-wk1', 'win-8', 'win-8.1-wk1', 'win-8.1', 'win-win10-wk1', 'win-win10', 'win-wk1', 'win'], Version.from_name('XP'))
        self._assert_search_path(['win-vista-wk1', 'win-vista', 'win-7sp0-wk1', 'win-7sp0', 'win-8-wk1', 'win-8', 'win-8.1-wk1', 'win-8.1', 'win-win10-wk1', 'win-win10', 'win-wk1', 'win'], Version.from_name('Vista'))
        self._assert_search_path(['win-7sp0-wk1', 'win-7sp0', 'win-8-wk1', 'win-8', 'win-8.1-wk1', 'win-8.1', 'win-win10-wk1', 'win-win10', 'win-wk1', 'win'], Version.from_name('7sp0'))

        self._assert_search_path(['win-xp-wk2', 'win-xp', 'win-vista-wk2', 'win-vista', 'win-7sp0-wk2', 'win-7sp0', 'win-8-wk2', 'win-8', 'win-8.1-wk2', 'win-8.1', 'win-win10-wk2', 'win-win10', 'win-wk2', 'win', 'wk2'], Version.from_name('XP'), use_webkit2=True)
        self._assert_search_path(['win-vista-wk2', 'win-vista', 'win-7sp0-wk2', 'win-7sp0', 'win-8-wk2', 'win-8', 'win-8.1-wk2', 'win-8.1', 'win-win10-wk2', 'win-win10', 'win-wk2', 'win', 'wk2'], Version.from_name('Vista'), use_webkit2=True)
        self._assert_search_path(['win-7sp0-wk2', 'win-7sp0', 'win-8-wk2', 'win-8', 'win-8.1-wk2', 'win-8.1', 'win-win10-wk2', 'win-win10', 'win-wk2', 'win', 'wk2'], Version.from_name('7sp0'), use_webkit2=True)

    def _assert_version(self, port_name, expected_version):
        host = MockSystemHost(os_name='win', os_version=expected_version)
        port = WinPort(host, port_name=port_name)
        self.assertEqual(port.version_name(), VersionNameMap.map().to_name(expected_version, platform='win', table=PUBLIC_TABLE))

    def test_versions(self):
        self._assert_version('win-xp', Version.from_name('XP'))
        self._assert_version('win-vista', Version.from_name('Vista'))
        self._assert_version('win-7sp0', Version.from_name('7sp0'))
        self.assertRaises(AssertionError, self._assert_version, 'win-me', Version.from_name('Vista'))

    def test_compare_text(self):
        expected = "EDITING DELEGATE: webViewDidChangeSelection:WebViewDidChangeSelectionNotification\nfoo\nEDITING DELEGATE: webViewDidChangeSelection:WebViewDidChangeSelectionNotification\n"
        port = self.make_port()
        self.assertFalse(port.do_text_results_differ(expected, "foo\n"))
        self.assertTrue(port.do_text_results_differ(expected, "foo"))
        self.assertTrue(port.do_text_results_differ(expected, "bar"))

        # This hack doesn't exist in WK2.
        port._options = MockOptions(webkit_test_runner=True)
        self.assertTrue(port.do_text_results_differ(expected, "foo\n"))

    def test_operating_system(self):
        self.assertEqual('win', self.make_port().operating_system())

    def test_expectations_files(self):
        self.assertEqual(
            self.make_port().expectations_files(),
            [
                "/mock-checkout/LayoutTests/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-wk1/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-win10/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-win10-wk1/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-8.1/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-8.1-wk1/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-8/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-8-wk1/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-7sp0/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-7sp0-wk1/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-vista/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-vista-wk1/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-xp/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-xp-wk1/TestExpectations",
            ],
        )

        self.assertEqual(
            self.make_port(
                options=MockOptions(webkit_test_runner=True, configuration="Release")
            ).expectations_files(),
            [
                "/mock-checkout/LayoutTests/TestExpectations",
                "/mock-checkout/LayoutTests/platform/wk2/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-wk2/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-win10/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-win10-wk2/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-8.1/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-8.1-wk2/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-8/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-8-wk2/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-7sp0/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-7sp0-wk2/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-vista/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-vista-wk2/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-xp/TestExpectations",
                "/mock-checkout/LayoutTests/platform/win-xp-wk2/TestExpectations",
            ],
        )

    def test_get_crash_log(self):
        # Win crash logs are tested elsewhere, so here we just make sure we don't crash.
        def fake_time_cb():
            times = [0, 20, 40]
            return lambda: times.pop(0)
        port = self.make_port(port_name='win')
        port._get_crash_log('DumpRenderTree', 1234, '', '', 0,
            time_fn=fake_time_cb(), sleep_fn=lambda delay: None)

    def test_layout_test_searchpath_with_apple_additions(self):
        search_path = self.make_port().default_baseline_search_path()
        with port_testcase.bind_mock_apple_additions():
            additions_search_path = self.make_port().default_baseline_search_path()
        self.assertEqual(search_path, additions_search_path)

    def test_default_upload_configuration(self):
        port = self.make_port()
        configuration = port.configuration_for_upload()
        self.assertEqual(configuration['architecture'], port.architecture())
        self.assertEqual(configuration['is_simulator'], False)
        self.assertEqual(configuration['platform'], 'win')
        self.assertEqual(configuration['style'], 'release')
        self.assertEqual(configuration['version_name'], 'XP')
