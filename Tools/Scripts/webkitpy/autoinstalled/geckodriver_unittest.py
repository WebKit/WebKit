# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import unittest
from unittest.mock import patch

import requests

from webkitcorepy import AutoInstall, OutputCapture, Package, Version, mocks

# Importing the module installs geckodriver.
with patch.object(Package, 'install'):
    from webkitpy.autoinstalled.geckodriver import GeckoDriverPackage


@patch.object(AutoInstall, 'times_to_retry', new=1)
class GeckoDriverPackageTest(unittest.TestCase):
    LINK = 'https://github.com/mozilla/geckodriver/releases/download/v0.27.0/geckodriver-v0.27.0-macos.tar.gz'

    def test_releases(self):
        response = mocks.Response.fromJson([{'tag_name': 'v0.27.0', 'assets': [{'browser_download_url': self.LINK}]}])
        with patch('requests.get', return_value=response) as mock_get, patch('platform.system', return_value='Darwin'):
            archives = GeckoDriverPackage(version=Version(0, 27, 0)).archives()
        self.assertEqual([archive.link for archive in archives], [self.LINK])
        mock_get.assert_called_once_with(GeckoDriverPackage.URL, timeout=AutoInstall.timeout)

    def test_releases_unreachable(self):
        with patch('requests.get', side_effect=requests.ConnectionError('no network')) as mock_get, OutputCapture():
            with self.assertRaises(requests.ConnectionError):
                GeckoDriverPackage(version=Version(0, 27, 0)).archives()
        self.assertEqual(mock_get.call_count, 2)

    def test_releases_error_response(self):
        with patch('requests.get', return_value=mocks.Response(status_code=403)) as mock_get, OutputCapture():
            with self.assertRaisesRegex(IOError, 'response code 403'):
                GeckoDriverPackage(version=Version(0, 27, 0)).archives()
        self.assertEqual(mock_get.call_count, 2)
