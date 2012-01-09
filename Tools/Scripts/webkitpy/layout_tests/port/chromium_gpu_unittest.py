#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
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

import sys
import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.layout_tests.port import chromium_gpu
from webkitpy.layout_tests.port import port_testcase
from webkitpy.layout_tests.port.factory import PortFactory
from webkitpy.tool.mocktool import MockOptions


class ChromiumGpuTest(unittest.TestCase):
    def integration_test_chromium_gpu_linux(self):
        self.assert_port_works('chromium-gpu-linux')
        self.assert_port_works('chromium-gpu-linux', 'chromium-gpu', 'linux2')
        self.assert_port_works('chromium-gpu-linux', 'chromium-gpu', 'linux3')

    def integration_test_chromium_gpu_mac(self):
        self.assert_port_works('chromium-gpu-mac')
        self.assert_port_works('chromium-gpu-mac', 'chromium-gpu', 'darwin')

    def integration_test_chromium_gpu_win(self):
        self.assert_port_works('chromium-gpu-win')
        self.assert_port_works('chromium-gpu-win', 'chromium-gpu', 'win32')
        self.assert_port_works('chromium-gpu-win', 'chromium-gpu', 'cygwin')

    def assert_port_works(self, port_name, input_name=None, platform=None):
        host = MockHost()
        host.filesystem = FileSystem()  # FIXME: This test should not use a real filesystem!

        # test that we got the right port
        mock_options = MockOptions(accelerated_2d_canvas=None,
                                   accelerated_video=None,
                                   builder_name='foo',
                                   child_processes=None)
        if input_name and platform:
            port = PortFactory(host).get(host, platform=platform, port_name=input_name, options=mock_options)
        else:
            port = PortFactory(host).get(host, port_name=port_name, options=mock_options)
        self.assertTrue(port._options.accelerated_2d_canvas)
        self.assertTrue(port._options.accelerated_video)
        self.assertTrue(port._options.experimental_fully_parallel)
        self.assertEqual(port._options.builder_name, 'foo - GPU')

        self.assertTrue(port.name().startswith(port_name))

        # test that it has the right directories in front of the search path.
        paths = port.baseline_search_path()
        self.assertEqual(port._webkit_baseline_path(port_name), paths[0])
        if port_name == 'chromium-gpu-linux':
            self.assertEqual(port._webkit_baseline_path('chromium-gpu-win'), paths[1])
            self.assertEqual(port._webkit_baseline_path('chromium-gpu'), paths[2])
        else:
            self.assertEqual(port._webkit_baseline_path('chromium-gpu'), paths[1])

        # Test that we're limiting to the correct directories.
        # These two tests are picked mostly at random, but we make sure they
        # exist separately from being filtered out by the port.

        # Note that this is using a real filesystem.
        files = port.tests(None)

        path = 'fast/html/keygen.html'
        self.assertTrue(port._filesystem.exists(port.abspath_for_test(path)))
        self.assertFalse(path in files)

    def _assert_baseline_path(self, port_name, baseline_path):
        host = MockHost()
        port = host.port_factory.get(port_name)
        self.assertEquals(port.name(), port_name)
        self.assertEquals(port.baseline_path(), port._webkit_baseline_path(baseline_path))

    def test_baseline_paths(self):
        self._assert_baseline_path('chromium-gpu-win-vista', 'chromium-gpu-win')
        self._assert_baseline_path('chromium-gpu-win-xp', 'chromium-gpu-win')
        self._assert_baseline_path('chromium-gpu-win-win7', 'chromium-gpu-win')

    def test_graphics_type(self):
        host = MockHost()
        port = host.port_factory.get('chromium-gpu-mac')
        self.assertEquals('gpu', port.graphics_type())

    def test_default_tests_paths(self):
        host = MockHost()

        def test_paths(port_name):
            return chromium_gpu._default_tests_paths(host.port_factory.get(port_name))

        self.assertEqual(test_paths('chromium-gpu-linux'), ['media', 'fast/canvas', 'canvas/philip'])
        self.assertEqual(test_paths('chromium-gpu-mac-leopard'), ['fast/canvas', 'canvas/philip'])

    def test_test_files(self):
        host = MockHost()
        files = {
            '/mock-checkout/LayoutTests/canvas/philip/test.html': '',
            '/mock-checkout/LayoutTests/fast/canvas/test.html': '',
            '/mock-checkout/LayoutTests/fast/html/test.html': '',
            '/mock-checkout/LayoutTests/media/test.html': '',
            '/mock-checkout/LayoutTests/foo/bar.html': '',
        }
        host.filesystem = MockFileSystem(files)

        def test_paths(port_name):
            return host.port_factory.get(port_name).tests([])

        self.assertEqual(test_paths('chromium-gpu-linux'), set(['canvas/philip/test.html', 'fast/canvas/test.html', 'media/test.html']))
        self.assertEqual(test_paths('chromium-gpu-mac-leopard'), set(['canvas/philip/test.html', 'fast/canvas/test.html']))


if __name__ == '__main__':
    port_testcase.main()
