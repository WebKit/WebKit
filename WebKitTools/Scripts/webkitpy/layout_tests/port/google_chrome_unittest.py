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
import google_chrome


class GetGoogleChromePortTest(unittest.TestCase):
    def test_get_google_chrome_port(self):
        test_ports = ('google-chrome-linux32', 'google-chrome-linux64',
            'google-chrome-mac', 'google-chrome-win')
        for port in test_ports:
            self._verify_baseline_path(port, port)

        self._verify_baseline_path('google-chrome-mac', 'google-chrome-mac-leopard')
        self._verify_baseline_path('google-chrome-win', 'google-chrome-win-xp')
        self._verify_baseline_path('google-chrome-win', 'google-chrome-win-vista')

    def _verify_baseline_path(self, expected_path, port_name):
        port = google_chrome.GetGoogleChromePort(port_name=port_name,
                                                 options=None)
        path = port.baseline_search_path()[0]
        self.assertEqual(expected_path, os.path.split(path)[1])
