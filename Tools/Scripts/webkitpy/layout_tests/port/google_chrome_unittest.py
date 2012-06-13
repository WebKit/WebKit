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

from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.layout_tests.port.factory import PortFactory


class TestGoogleChromePort(unittest.TestCase):
    def _verify_baseline_search_path_startswith(self, port_name, expected_platform_dirs):
        port = PortFactory(MockSystemHost()).get(port_name=port_name)
        actual_platform_dirs = [port._filesystem.basename(path) for path in port.baseline_search_path()]
        self.assertEqual(expected_platform_dirs, actual_platform_dirs[0:len(expected_platform_dirs)])

    def _verify_expectations_overrides(self, port_name):
        host = MockSystemHost()
        port = PortFactory(host).get(port_name=port_name, options=None)
        self.assertTrue('TestExpectations' in port.expectations_files()[0])
        self.assertTrue('skia_test_expectations.txt' in port.expectations_files()[1])
        self.assertTrue('test_expectations_chrome.txt' in port.expectations_files()[-1])

    def test_get_google_chrome_port(self):
        self._verify_baseline_search_path_startswith('google-chrome-linux32', ['google-chrome-linux32', 'chromium-linux-x86'])
        self._verify_baseline_search_path_startswith('google-chrome-linux64', ['google-chrome-linux64', 'chromium-linux'])
        self._verify_baseline_search_path_startswith('google-chrome-mac', ['google-chrome-mac', 'chromium-mac-snowleopard'])
        self._verify_baseline_search_path_startswith('google-chrome-win', ['google-chrome-win', 'chromium-win'])

        self._verify_expectations_overrides('google-chrome-mac')
        self._verify_expectations_overrides('google-chrome-win')
        self._verify_expectations_overrides('google-chrome-linux32')
        self._verify_expectations_overrides('google-chrome-linux64')


if __name__ == '__main__':
    unittest.main()
