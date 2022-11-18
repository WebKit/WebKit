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
import os
import sys
import unittest

from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.port.config import clear_cached_configuration
from webkitpy.port.gtk import GtkPort
from webkitpy.port import port_testcase
from webkitpy.thirdparty.mock import Mock
from webkitpy.tool.mocktool import MockOptions

from webkitcorepy import OutputCapture


class GtkPortTest(port_testcase.PortTestCase):
    port_name = 'gtk'
    port_maker = GtkPort

    def test_default_baseline_search_path(self):
        port = self.make_port()
        self.assertEqual(port.default_baseline_search_path(),
                         ['/mock-checkout/LayoutTests/platform/gtk',
                          '/mock-checkout/LayoutTests/platform/glib',
                          '/mock-checkout/LayoutTests/platform/wk2'])

    def test_port_specific_expectations_files(self):
        port = self.make_port()
        self.assertEqual(port.expectations_files(),
                         ['/mock-checkout/LayoutTests/TestExpectations',
                          '/mock-checkout/LayoutTests/platform/wk2/TestExpectations',
                          '/mock-checkout/LayoutTests/platform/glib/TestExpectations',
                          '/mock-checkout/LayoutTests/platform/gtk/TestExpectations'])

    def test_show_results_html_file(self):
        port = self.make_port()
        port._executive = MockExecutive(should_log=True)
        port._filesystem = MockFileSystem({
            "/mock-build/bin/MiniBrowser": ""
        })
        with OutputCapture(level=logging.INFO) as captured:
            port.show_results_html_file('test.html')
            mock_command, mock_env = captured.root.log.getvalue().split(' env=')
        self.assertEqual(
            mock_command,
            "MOCK run_command: ['/mock-build/bin/MiniBrowser', 'file://test.html'], cwd=/mock-checkout,"
        )
        # Check the environment variables defined by port.setup_environ_for_minibrowser()
        for mb_env_var in ['LD_LIBRARY_PATH', 'WEBKIT_INJECTED_BUNDLE_PATH', 'WEBKIT_EXEC_PATH']:
            self.assertTrue(mb_env_var in mock_env)

    def test_default_timeout_ms(self):
        self.assertEqual(self.make_port(options=MockOptions(configuration='Release')).default_timeout_ms(), 15000)
        self.assertEqual(self.make_port(options=MockOptions(configuration='Debug')).default_timeout_ms(), 30000)
        self.assertEqual(self.make_port(options=MockOptions(configuration='Release', leaks=True, wrapper="valgrind")).default_timeout_ms(), 150000)
        self.assertEqual(self.make_port(options=MockOptions(configuration='Debug', leaks=True, wrapper="valgrind")).default_timeout_ms(), 300000)

    def test_get_crash_log(self):
        # This function tested in linux_get_crash_log_unittest.py
        pass

    def test_default_upload_configuration(self):
        clear_cached_configuration()
        port = self.make_port()
        configuration = port.configuration_for_upload()
        self.assertEqual(configuration['architecture'], port.architecture())
        self.assertEqual(configuration['is_simulator'], False)
        self.assertEqual(configuration['platform'], 'GTK')
        self.assertEqual(configuration['style'], 'release')
        self.assertEqual(configuration['version_name'], 'Xvfb')

    def test_gtk4_expectations_binary_only(self):
        port = self.make_port()
        port._filesystem = MockFileSystem({
            "/mock-build/lib/libwebkit2gtk-5.0.so": ""
        })
        with OutputCapture() as _:
            self.assertEqual(port.expectations_files(),
                              ['/mock-checkout/LayoutTests/TestExpectations',
                               '/mock-checkout/LayoutTests/platform/wk2/TestExpectations',
                               '/mock-checkout/LayoutTests/platform/glib/TestExpectations',
                               '/mock-checkout/LayoutTests/platform/gtk/TestExpectations',
                               '/mock-checkout/LayoutTests/platform/gtk4/TestExpectations'])

    def test_gtk3_expectations_binary_only(self):
        port = self.make_port()
        port._filesystem = MockFileSystem({
            "/mock-build/lib/libwebkit2gtk-4.0.so": ""
        })

        with OutputCapture() as _:
            self.assertEqual(port.expectations_files(),
                              ['/mock-checkout/LayoutTests/TestExpectations',
                               '/mock-checkout/LayoutTests/platform/wk2/TestExpectations',
                               '/mock-checkout/LayoutTests/platform/glib/TestExpectations',
                               '/mock-checkout/LayoutTests/platform/gtk/TestExpectations'])

    def test_gtk_expectations_both_binaries(self):
        port = self.make_port()
        port._filesystem = MockFileSystem({
            "/mock-build/lib/libwebkit2gtk-4.0.so": "",
            "/mock-build/lib/libwebkit2gtk-5.0.so": ""
        })

        with OutputCapture() as captured:
            self.assertEqual(port.expectations_files(),
                              ['/mock-checkout/LayoutTests/TestExpectations',
                               '/mock-checkout/LayoutTests/platform/wk2/TestExpectations',
                               '/mock-checkout/LayoutTests/platform/glib/TestExpectations',
                               '/mock-checkout/LayoutTests/platform/gtk/TestExpectations'])
            self.assertEqual(captured.root.log.getvalue(), 'Multiple WebKit2GTK libraries found. Skipping GTK4 detection.\n')
