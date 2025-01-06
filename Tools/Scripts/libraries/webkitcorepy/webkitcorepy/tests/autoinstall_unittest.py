# Copyright (C) 2024 Apple Inc. All rights reserved.
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
from urllib.error import URLError

from webkitcorepy import autoinstall, OutputCapture
from webkitcorepy.autoinstall import AutoInstall, Package
from webkitcorepy.version import Version


class ArchiveTest(unittest.TestCase):
    @patch.object(AutoInstall, "_verify_index", autospec=True, return_value=None)
    @patch.object(
        autoinstall, "urlopen", autospec=True, side_effect=URLError("no network")
    )
    @patch.object(AutoInstall, "times_to_retry", new=3)
    def test_retry(self, mock_urlopen, mock_verify_index):
        with OutputCapture():
            archive = Package.Archive(
                "dummy", "http://example.example/dummy-1.0-py3-none-any.whl", Version(1, 0)
            )
            with self.assertRaises(URLError):
                archive.download()
            self.assertEqual(mock_urlopen.call_count, 4)
