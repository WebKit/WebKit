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

import chromium_gpu

from webkitpy.tool.mocktool import MockOptions
from webkitpy.common.host_mock import MockHost
from webkitpy.layout_tests.port import port_testcase


class ChromiumGpuTest(unittest.TestCase):
    def integration_test_chromium_gpu_linux(self):
        if sys.platform not in ('linux2', 'linux3'):
            return
        self.assert_port_works('chromium-gpu-linux')
        self.assert_port_works('chromium-gpu-linux', 'chromium-gpu', 'linux2')
        self.assert_port_works('chromium-gpu-linux', 'chromium-gpu', 'linux3')

    def integration_test_chromium_gpu_mac(self):
        if sys.platform != 'darwin':
            return
        self.assert_port_works('chromium-gpu-cg-mac')
        self.assert_port_works('chromium-gpu-mac')
        # For now, chromium-gpu on Mac defaults to the chromium-gpu-cg-mac port.
        self.assert_port_works('chromium-gpu-cg-mac', 'chromium-gpu', 'darwin')

    def integration_test_chromium_gpu_win(self):
        if sys.platform not in ('cygwin', 'win32'):
            return
        self.assert_port_works('chromium-gpu-win')
        self.assert_port_works('chromium-gpu-win', 'chromium-gpu', 'win32')
        self.assert_port_works('chromium-gpu-win', 'chromium-gpu', 'cygwin')

    def assert_port_works(self, port_name, input_name=None, platform=None):
        host = MockHost()
        host.filesystem = FileSystem()  # FIXME: This test should not use a real filesystem!

        # test that we got the right port
        mock_options = MockOptions(accelerated_compositing=None,
                                            accelerated_2d_canvas=None,
                                            builder_name='foo',
                                            child_processes=None)
        if input_name and platform:
            port = chromium_gpu.get(host, platform=platform, port_name=input_name, options=mock_options)
        else:
            port = chromium_gpu.get(host, port_name=port_name, options=mock_options)
        self.assertTrue(port._options.accelerated_compositing)
        self.assertTrue(port._options.accelerated_2d_canvas)
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

        path = 'compositing/checkerboard.html'
        self.assertTrue(port._filesystem.exists(port.abspath_for_test(path)))
        self.assertTrue(path in files)

        path = 'fast/html/keygen.html'
        self.assertTrue(port._filesystem.exists(port.abspath_for_test(path)))
        self.assertFalse(path in files)
        if port_name.startswith('chromium-gpu-cg-mac'):
            path = 'fast/canvas/set-colors.html'
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
        self._assert_baseline_path('chromium-gpu-cg-mac-leopard', 'chromium-gpu-cg-mac')
        self._assert_baseline_path('chromium-gpu-cg-mac-snowleopard', 'chromium-gpu-cg-mac')

    def test_graphics_type(self):
        host = MockHost()
        port = host.port_factory.get('chromium-gpu-cg-mac')
        self.assertEquals('gpu-cg', port.graphics_type())
        port = host.port_factory.get('chromium-gpu-mac')
        self.assertEquals('gpu', port.graphics_type())


if __name__ == '__main__':
    port_testcase.main()
