# Copyright (C) 2019 Apple Inc. All rights reserved.
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

import mock
import time
import unittest

from resultsdbpy.model.casserole import CasseroleNodes


class MockRequest(object):

    def __init__(self, text='', status_code=200):
        time.sleep(.1)  # This split second acts like an actual request
        self.text = text
        self.status_code = status_code


class CasseroleTest(unittest.TestCase):
    def test_synchronous(self):
        with mock.patch('requests.get', new=lambda *args, **kwargs: MockRequest('start')):
            nodes = CasseroleNodes('some-url', interval_seconds=10, asynchronous=False)
            self.assertEqual(['start'], nodes.nodes)

            with mock.patch('requests.get', new=lambda *args, **kwargs: MockRequest('url1,url2')):
                self.assertEqual(['start'], nodes.nodes)
                current_time = time.time()

                with mock.patch('time.time', new=lambda: current_time + 15):
                    self.assertEqual(['url1', 'url2'], nodes.nodes)

    def test_asynchronous(self):
        with mock.patch('requests.get', new=lambda *args, **kwargs: MockRequest('start')):
            nodes = CasseroleNodes('some-url', interval_seconds=10, asynchronous=True)
            self.assertEqual(['start'], nodes.nodes)

            with mock.patch('requests.get', new=lambda *args, **kwargs: MockRequest('url1,url2')):
                self.assertEqual(['start'], nodes.nodes)
                current_time = time.time()

                with mock.patch('time.time', new=lambda: current_time + 15):
                    self.assertEqual(['start'], nodes.nodes)
                    time.sleep(.15)  # Wait for the asynchronous thread to finish
                    self.assertEqual(['url1', 'url2'], nodes.nodes)

    def test_list_like(self):
        with mock.patch('requests.get', new=lambda *args, **kwargs: MockRequest('url1,url2,url3')):
            nodes = CasseroleNodes('some-url')
            self.assertEqual(['url1', 'url2', 'url3'], [node for node in nodes])
            self.assertTrue(nodes)
            self.assertTrue('url1' in nodes)
