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

from webkitpy.common.host_mock import MockHost
from webkitpy.layout_tests.port import google_chrome


class GetGoogleChromePortTest(unittest.TestCase):
    def test_get_google_chrome_port(self):
        test_ports = ('google-chrome-linux32', 'google-chrome-linux64', 'google-chrome-mac', 'google-chrome-win')
        for port in test_ports:
            self._verify_baseline_path(port, port)
            self._verify_expectations_overrides(port)

        self._verify_baseline_path('google-chrome-mac', 'google-chrome-mac-leopard')
        self._verify_baseline_path('google-chrome-win', 'google-chrome-win-xp')
        self._verify_baseline_path('google-chrome-win', 'google-chrome-win-vista')

    def _verify_baseline_path(self, expected_path, port_name):
        port = google_chrome.GetGoogleChromePort(MockHost(), port_name=port_name)
        path = port.baseline_search_path()[0]
        self.assertEqual(expected_path, port._filesystem.basename(path))

    def _verify_expectations_overrides(self, port_name):
        # FIXME: Make this more robust when we have the Tree() abstraction.
        # we should be able to test for the files existing or not, and
        # be able to control the contents better.
        # FIXME: What is the Tree() abstraction?

        host = MockHost()
        chromium_port = host.port_factory.get("chromium-cg-mac")
        chromium_base = chromium_port.path_from_chromium_base()
        port = google_chrome.GetGoogleChromePort(host, port_name=port_name, options=None)

        expected_chromium_overrides = '// chromium overrides\n'
        expected_chrome_overrides = '// chrome overrides\n'
        chromium_path = host.filesystem.join(chromium_base, 'webkit', 'tools', 'layout_tests', 'test_expectations.txt')
        chrome_path = host.filesystem.join(chromium_base, 'webkit', 'tools', 'layout_tests', 'test_expectations_chrome.txt')

        host.filesystem.files[chromium_path] = expected_chromium_overrides
        host.filesystem.files[chrome_path] = None
        actual_chrome_overrides = port.test_expectations_overrides()
        self.assertEqual(expected_chromium_overrides, actual_chrome_overrides)

        host.filesystem.files[chrome_path] = expected_chrome_overrides
        actual_chrome_overrides = port.test_expectations_overrides()
        self.assertEqual(actual_chrome_overrides, expected_chromium_overrides + expected_chrome_overrides)


if __name__ == '__main__':
    unittest.main()
