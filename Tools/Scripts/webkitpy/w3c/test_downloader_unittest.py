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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import unittest

from webkitpy.w3c.test_downloader import TestDownloader


class TestDownloaderProtocolTest(unittest.TestCase):
    def test_protocol_from_url_https(self):
        self.assertEqual(TestDownloader._protocol_from_url('https://github.com/web-platform-tests/wpt.git'), 'https')

    def test_protocol_from_url_http(self):
        self.assertEqual(TestDownloader._protocol_from_url('http://github.com/web-platform-tests/wpt.git'), 'http')

    def test_protocol_from_url_git(self):
        self.assertEqual(TestDownloader._protocol_from_url('git://github.com/web-platform-tests/wpt.git'), 'git')

    def test_protocol_from_url_ssh_scheme(self):
        self.assertEqual(TestDownloader._protocol_from_url('ssh://git@github.com/web-platform-tests/wpt.git'), 'ssh')

    def test_protocol_from_url_scp_like(self):
        self.assertEqual(TestDownloader._protocol_from_url('git@github.com:web-platform-tests/wpt.git'), 'ssh')

    def test_protocol_from_url_scp_like_with_user(self):
        self.assertEqual(TestDownloader._protocol_from_url('user@host.com:path/to/repo.git'), 'ssh')

    def test_protocol_from_url_ftp(self):
        self.assertEqual(TestDownloader._protocol_from_url('ftp://example.com/repo.git'), 'ftp')

    def test_protocol_from_url_ftps(self):
        self.assertEqual(TestDownloader._protocol_from_url('ftps://example.com/repo.git'), 'ftps')

    def test_protocol_from_url_file(self):
        self.assertEqual(TestDownloader._protocol_from_url('file:///path/to/repo.git'), 'file')

    def test_protocol_from_url_local_path(self):
        self.assertIsNone(TestDownloader._protocol_from_url('/path/to/repo'))

    def test_normalize_url_protocol_https_to_https(self):
        url = 'https://github.com/web-platform-tests/wpt.git'
        self.assertEqual(TestDownloader._normalize_url_protocol(url, 'https'), url)

    def test_normalize_url_protocol_ssh_to_ssh(self):
        url = 'git@github.com:web-platform-tests/wpt.git'
        self.assertEqual(TestDownloader._normalize_url_protocol(url, 'ssh'), url)

    def test_normalize_url_protocol_https_to_ssh(self):
        url = 'https://github.com/web-platform-tests/wpt.git'
        self.assertEqual(TestDownloader._normalize_url_protocol(url, 'ssh'), 'git@github.com:web-platform-tests/wpt.git')

    def test_normalize_url_protocol_ssh_to_https(self):
        url = 'git@github.com:web-platform-tests/wpt.git'
        self.assertEqual(TestDownloader._normalize_url_protocol(url, 'https'), 'https://github.com/web-platform-tests/wpt.git')

    def test_normalize_url_protocol_git_to_https(self):
        url = 'git://github.com/web-platform-tests/wpt.git'
        self.assertEqual(TestDownloader._normalize_url_protocol(url, 'https'), 'https://github.com/web-platform-tests/wpt.git')

    def test_normalize_url_protocol_ssh_scheme_to_https(self):
        url = 'ssh://git@github.com/web-platform-tests/wpt.git'
        self.assertEqual(TestDownloader._normalize_url_protocol(url, 'https'), 'https://github.com/web-platform-tests/wpt.git')

    def test_normalize_url_protocol_preserves_path(self):
        url = 'https://github.com/org/project/repo.git'
        self.assertEqual(TestDownloader._normalize_url_protocol(url, 'ssh'), 'git@github.com:org/project/repo.git')

    def test_normalize_url_protocol_no_change_needed(self):
        url = '/local/path'
        self.assertEqual(TestDownloader._normalize_url_protocol(url, 'https'), url)
