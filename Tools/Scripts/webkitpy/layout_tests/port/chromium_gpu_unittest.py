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

import unittest

from webkitpy.tool import mocktool
import chromium_gpu

from webkitpy.layout_tests.port import factory

class ChromiumGpuTest(unittest.TestCase):
    def test_get_chromium_gpu_linux(self):
        self.assertOverridesWorked('chromium-gpu-linux')

    def test_get_chromium_gpu_mac(self):
        self.assertOverridesWorked('chromium-gpu-mac')

    def test_get_chromium_gpu_win(self):
        self.assertOverridesWorked('chromium-gpu-win')

    def test_get_chromium_gpu__on_linux(self):
        self.assertOverridesWorked('chromium-gpu-linux', 'chromium-gpu', 'linux2')

    def test_get_chromium_gpu__on_mac(self):
        self.assertOverridesWorked('chromium-gpu-mac', 'chromium-gpu', 'darwin')

    def test_get_chromium_gpu__on_win(self):
        self.assertOverridesWorked('chromium-gpu-win', 'chromium-gpu', 'win32')
        self.assertOverridesWorked('chromium-gpu-win', 'chromium-gpu', 'cygwin')

    def assertOverridesWorked(self, port_name, input_name=None, platform=None):
        # test that we got the right port
        mock_options = mocktool.MockOptions(accelerated_compositing=None,
                                            accelerated_2d_canvas=None,
                                            builder_name='foo',
                                            child_processes=None)
        if input_name and platform:
            port = chromium_gpu.get(platform=platform, port_name=input_name,
                                    options=mock_options)
        else:
            port = chromium_gpu.get(port_name=port_name, options=mock_options)
        self.assertTrue(port._options.accelerated_compositing)
        self.assertTrue(port._options.accelerated_2d_canvas)
        self.assertEqual(port.default_child_processes(), 1)
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
        files = port.tests(None)

        path = port.abspath_for_test('compositing/checkerboard.html')
        self.assertTrue(port._filesystem.exists(path))
        self.assertTrue(path in files)

        path = port.abspath_for_test('fast/html/keygen.html')
        self.assertTrue(port._filesystem.exists(path))
        self.assertFalse(path in files)
        if port_name.startswith('chromium-gpu-mac'):
            path = port.abspath_for_test('fast/canvas/set-colors.html')
            self.assertTrue(port._filesystem.exists(path))
            self.assertFalse(path in files)

    def test_chromium_gpu__vista(self):
        port = factory.get('chromium-gpu-win-vista')
        self.assertEquals(port.name(), 'chromium-gpu-win-vista')
        self.assertEquals(port.baseline_path(), port._webkit_baseline_path('chromium-gpu-win'))

    def test_chromium_gpu__xp(self):
        port = factory.get('chromium-gpu-win-xp')
        self.assertEquals(port.name(), 'chromium-gpu-win-xp')
        self.assertEquals(port.baseline_path(), port._webkit_baseline_path('chromium-gpu-win'))

    def test_chromium_gpu__win7(self):
        port = factory.get('chromium-gpu-win-win7')
        self.assertEquals(port.name(), 'chromium-gpu-win-win7')
        self.assertEquals(port.baseline_path(), port._webkit_baseline_path('chromium-gpu-win'))

    def test_chromium_gpu__leopard(self):
        port = factory.get('chromium-gpu-mac-leopard')
        self.assertEquals(port.name(), 'chromium-gpu-mac-leopard')
        self.assertEquals(port.baseline_path(), port._webkit_baseline_path('chromium-gpu-mac'))

    def test_chromium_gpu__snowleopard(self):
        port = factory.get('chromium-gpu-mac-snowleopard')
        self.assertEquals(port.name(), 'chromium-gpu-mac-snowleopard')
        self.assertEquals(port.baseline_path(), port._webkit_baseline_path('chromium-gpu-mac'))


if __name__ == '__main__':
    unittest.main()
