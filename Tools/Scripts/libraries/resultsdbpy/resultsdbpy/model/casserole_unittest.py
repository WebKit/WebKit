# Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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

import time
import unittest

from resultsdbpy.model.casserole import CasseroleNodes
from webkitcorepy import mocks


class CasseroleTest(unittest.TestCase):
    def test_synchronous(self):
        with mocks.Requests('casserole.webkit.org', **{
            'api/cluster-endpoints': mocks.Response(text='start')
        }), mocks.Time:
            nodes = CasseroleNodes('https://casserole.webkit.org/api/cluster-endpoints', interval_seconds=10, asynchronous=False)
            self.assertEqual(['start'], nodes.nodes)

            with mocks.Requests('casserole.webkit.org', **{
                'api/cluster-endpoints': mocks.Response(text='url1,url2')
            }):
                self.assertEqual(['start'], nodes.nodes)
                time.sleep(15)
                self.assertEqual(['url1', 'url2'], nodes.nodes)

    def test_asynchronous(self):
        with mocks.Requests('casserole.webkit.org', **{
            'api/cluster-endpoints': mocks.Response(text='start')
        }), mocks.Time:
            nodes = CasseroleNodes('https://casserole.webkit.org/api/cluster-endpoints', interval_seconds=10, asynchronous=True)
            self.assertEqual(['start'], nodes.nodes)

        with mocks.Requests('casserole.webkit.org', **{
            'api/cluster-endpoints': mocks.Response(text='url1,url2')
        }):
            nodes = CasseroleNodes('https://casserole.webkit.org/api/cluster-endpoints', interval_seconds=10, asynchronous=True)
            self.assertEqual(['url1', 'url2'], nodes.nodes)

    def test_list_like(self):
        with mocks.Requests('casserole.webkit.org', **{
            'api/cluster-endpoints': mocks.Response(text='url1,url2,url3')
        }):
            nodes = CasseroleNodes('https://casserole.webkit.org/api/cluster-endpoints')
            self.assertEqual(['url1', 'url2', 'url3'], [node for node in nodes])
            self.assertTrue(nodes)
            self.assertTrue('url1' in nodes)
