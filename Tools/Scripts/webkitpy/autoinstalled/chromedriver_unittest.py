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

# Importing the module installs chromedriver.
with patch.object(Package, 'install'):
    from webkitpy.autoinstalled.chromedriver import ChromeDriverPackage


@patch.object(AutoInstall, 'times_to_retry', new=1)
class ChromeDriverPackageTest(unittest.TestCase):
    def test_latest_release(self):
        with patch('requests.get', return_value=mocks.Response.fromText('114.0.5735.90\n')) as mock_get, \
                patch('platform.system', return_value='Darwin'):
            archives = ChromeDriverPackage(version=Version(86, 0, 4240, 22)).archives()
        self.assertEqual(
            [archive.link for archive in archives],
            ['https://chromedriver.storage.googleapis.com/114.0.5735.90/chromedriver_mac64.zip'],
        )
        mock_get.assert_called_once_with(ChromeDriverPackage.URL + 'LATEST_RELEASE', timeout=AutoInstall.timeout)

    def test_latest_release_unreachable(self):
        with patch('requests.get', side_effect=requests.ConnectionError('no network')) as mock_get, OutputCapture():
            with self.assertRaises(requests.ConnectionError):
                ChromeDriverPackage(version=Version(86, 0, 4240, 22)).archives()
        self.assertEqual(mock_get.call_count, 2)

    def test_latest_release_error_response(self):
        with patch('requests.get', return_value=mocks.Response(status_code=503)) as mock_get, OutputCapture():
            with self.assertRaisesRegex(IOError, 'response code 503'):
                ChromeDriverPackage(version=Version(86, 0, 4240, 22)).archives()
        self.assertEqual(mock_get.call_count, 2)
