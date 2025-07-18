# Copyright (C) 2011 Google Inc. All rights reserved.
# Copyright (C) 2017 Igalia S.L. All rights reserved.
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
#    * Neither the name of the copyright holder nor the names of its
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

from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.port.wpe import WPEPort
from webkitpy.port import port_testcase
from webkitpy.thirdparty.mock import Mock, patch
from webkitpy.tool.mocktool import MockOptions
from webkitcorepy import OutputCapture
import logging

class WPEPortTest(port_testcase.PortTestCase):
    port_name = 'wpe'
    port_maker = WPEPort

    def _mock_port_cog_is_built(self, port):
        port._filesystem = MockFileSystem({
            '/mock-build/Tools/cog-prefix/src/cog-build/launcher/cog': '',
        })

    def test_default_baseline_search_path(self):
        port = self.make_port()
        self.assertEqual(port.default_baseline_search_path(),
                         ['/mock-checkout/LayoutTests/platform/wpe',
                          '/mock-checkout/LayoutTests/platform/glib',
                          '/mock-checkout/LayoutTests/platform/wk2'])

    def test_port_specific_expectations_files(self):
        port = self.make_port()
        self.assertEqual(port.expectations_files(),
                         ['/mock-checkout/LayoutTests/TestExpectations',
                          '/mock-checkout/LayoutTests/platform/wk2/TestExpectations',
                          '/mock-checkout/LayoutTests/platform/glib/TestExpectations',
                          '/mock-checkout/LayoutTests/platform/wpe/TestExpectations'])

    def test_port_legacy_api_specific_expectations_files(self):
        port = self.make_port(options=MockOptions(configuration='Release', wpe_legacy_api=True))
        self.assertEqual(port.expectations_files(),
                         ['/mock-checkout/LayoutTests/TestExpectations',
                          '/mock-checkout/LayoutTests/platform/wk2/TestExpectations',
                          '/mock-checkout/LayoutTests/platform/glib/TestExpectations',
                          '/mock-checkout/LayoutTests/platform/wpe/TestExpectations',
                          '/mock-checkout/LayoutTests/platform/wpe-legacy-api/TestExpectations'])

    def test_default_timeout_ms(self):
        self.assertEqual(self.make_port(options=MockOptions(configuration='Release')).default_timeout_ms(), 15000)
        self.assertEqual(self.make_port(options=MockOptions(configuration='Debug')).default_timeout_ms(), 30000)
        self.assertEqual(self.make_port(options=MockOptions(configuration='Release', leaks=True, wrapper="valgrind")).default_timeout_ms(), 150000)
        self.assertEqual(self.make_port(options=MockOptions(configuration='Debug', leaks=True, wrapper="valgrind")).default_timeout_ms(), 300000)

    def test_get_crash_log(self):
        # This function tested in linux_get_crash_log_unittest.py
        pass

    def test_default_upload_configuration(self):
        port = self.make_port()
        configuration = port.configuration_for_upload()
        self.assertEqual(configuration['architecture'], port.architecture())
        self.assertEqual(configuration['is_simulator'], False)
        self.assertEqual(configuration['platform'], 'WPE')
        self.assertEqual(configuration['style'], 'release')

    def test_browser_name_default_wihout_cog_built(self):
        port = self.make_port()
        self.assertEqual(port.browser_name(), "minibrowser")

    def test_browser_name_default_with_cog_built(self):
        port = self.make_port()
        self._mock_port_cog_is_built(port)
        with patch('os.environ', {}):
            self.assertEqual(port.browser_name(), "minibrowser")

    def test_browser_name_override_minibrowser_with_cog_built(self):
        with patch('os.environ', {'WPE_BROWSER': 'MiniBrowser'}):
            port = self.make_port()
            self._mock_port_cog_is_built(port)
            self.assertEqual(port.browser_name(), "minibrowser")

    def test_browser_name_override_cog_without_cog_built(self):
        with patch('os.environ', {'WPE_BROWSER': 'Cog'}):
            port = self.make_port()
            self.assertEqual(port.browser_name(), "cog")

    def test_browser_name_override_unknown_without_cog_built(self):
        with patch('os.environ', {'WPE_BROWSER': 'Mosaic'}):
            port = self.make_port()
            self.assertEqual(port.browser_name(), "minibrowser")

    def test_browser_name_override_unknown_with_cog_built(self):
        with patch('os.environ', {'WPE_BROWSER': 'Mosaic'}):
            port = self.make_port()
            self._mock_port_cog_is_built(port)
            self.assertEqual(port.browser_name(), "minibrowser")

    def test_browser_cog_parameters_platform_default(self):
        with patch('os.environ', {'WPE_BROWSER': 'cog'}):
            port = self.make_port()
            port._executive = MockExecutive(should_log=True)
            self._mock_port_cog_is_built(port)
            self.assertEqual(port.browser_name(), "cog")
            with OutputCapture(level=logging.DEBUG) as captured:
                url = 'https://url.com'
                port.run_minibrowser([url])
                mock_command = captured.root.log.getvalue()
                self.assertTrue(url in mock_command)
                self.assertTrue('--platform=gtk4' in mock_command)

    def test_browser_cog_parameters_platform_override_via_cmdline(self):
        with patch('os.environ', {'WPE_BROWSER': 'cog'}):
            port = self.make_port()
            port._executive = MockExecutive(should_log=True)
            self._mock_port_cog_is_built(port)
            self.assertEqual(port.browser_name(), "cog")
            with OutputCapture(level=logging.DEBUG) as captured:
                url = 'https://url.com'
                port.run_minibrowser(['-P', 'drm', url])
                mock_command = captured.root.log.getvalue()
                self.assertTrue(url in mock_command)
                self.assertFalse('--platform' in mock_command)
                self.assertTrue('-P' in mock_command)
                self.assertTrue('drm' in mock_command)

    def test_browser_cog_parameters_platform_override_via_environ(self):
        with patch('os.environ', {'WPE_BROWSER': 'cog', 'COG_PLATFORM_NAME': 'drm'}):
            port = self.make_port()
            port._executive = MockExecutive(should_log=True)
            self._mock_port_cog_is_built(port)
            self.assertEqual(port.browser_name(), "cog")
            with OutputCapture(level=logging.DEBUG) as captured:
                url = 'https://url.com'
                port.run_minibrowser([url])
                mock_command = captured.root.log.getvalue()
                self.assertTrue(url in mock_command)
                self.assertFalse('--platform' in mock_command)
                self.assertFalse('-P' in mock_command)

    def test_get_browser_path(self):
        port = self.make_port()
        self._mock_port_cog_is_built(port)
        # do not rename or remove port.get_browser_path() without also
        # updating webkitpy/browserperfdash/plans/browser_binary_size.py
        mb_path = port.get_browser_path('MiniBrowser')
        self.assertTrue(mb_path.endswith('/MiniBrowser'))
        cog_path = port.get_browser_path('cog')
        self.assertTrue(cog_path.endswith('/cog'))
