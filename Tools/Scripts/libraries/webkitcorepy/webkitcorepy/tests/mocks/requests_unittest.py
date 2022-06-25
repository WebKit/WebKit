# Copyright (C) 2020 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
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

import requests
import unittest

from webkitcorepy import mocks


class MockRequests(unittest.TestCase):

    class Example(mocks.Requests):
        def request(self, method, url, **kwargs):
            if url in ['https://{}/text'.format(host) for host in self.hosts]:
                return mocks.Response.fromText(data='text content', url=url)
            if url in ['https://{}/json'.format(host) for host in self.hosts]:
                return mocks.Response.fromJson(data=dict(content='json'), url=url)
            return mocks.Response.create404(url)

    def test_basic(self):
        with mocks.Requests('webkit.org'):
            self.assertEqual(requests.get('https://webkit.org').status_code, 404)

    def test_fallback(self):
        with mocks.Requests('webkit.org'):
            with mocks.Requests('bugs.webkit.org'):
                self.assertEqual(requests.get('https://webkit.org').status_code, 404)

    def test_text(self):
        with self.Example('webkit.org'):
            response = requests.get('https://webkit.org/text')
            self.assertEqual(response.status_code, 200)
            self.assertEqual(response.text, 'text content')
            self.assertDictEqual(response.headers, {'Content-Length': 12, 'Content-Type': 'text'})

    def test_json(self):
        with self.Example('webkit.org'):
            response = requests.get('https://webkit.org/json')
            self.assertEqual(response.status_code, 200)
            self.assertDictEqual(response.json(), dict(content='json'))
            self.assertDictEqual(response.headers, {'Content-Length': 19, 'Content-Type': 'text/json'})
