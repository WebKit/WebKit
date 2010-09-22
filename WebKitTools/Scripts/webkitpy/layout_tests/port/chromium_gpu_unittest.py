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

import os
import unittest
import chromium_gpu


class ChromiumGpuTest(unittest.TestCase):
    def test_get_chromium_gpu_linux(self):
        self.assertOverridesWorked('chromium-gpu-linux')

    def test_get_chromium_gpu_mac(self):
        self.assertOverridesWorked('chromium-gpu-mac')

    def test_get_chromium_gpu_win(self):
        self.assertOverridesWorked('chromium-gpu-win')

    def assertOverridesWorked(self, port_name):
        # test that we got the right port
        port = chromium_gpu.get(port_name=port_name, options=None)

        # we use startswith() instead of Equal to gloss over platform versions.
        self.assertTrue(port.name().startswith(port_name))

        # test that it has the right directory in front of the search path.
        path = port.baseline_search_path()[0]
        self.assertEqual(port._webkit_baseline_path(port_name), path)

        # test that we have the right expectations file.
        self.assertTrue('chromium-gpu' in
                        port.path_to_test_expectations_file())

if __name__ == '__main__':
    unittest.main()
