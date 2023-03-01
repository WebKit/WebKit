# Copyright (C) 2023 Apple Inc. All rights reserved.
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


class DummySession(object):
    PROXIES = None

    def __init__(self):
        DummySession.PROXIES = None

    def request(self, method, url, **kwargs):
        DummySession.PROXIES = kwargs.get('proxies', None)


class PartialProxyTest(unittest.TestCase):
    def test_session(self):
        # Some imports must be done within mock contexts because we're attempting to
        # test an override of request.Session with an override of request.Session
        from mock import patch

        with patch('requests.Session', new=DummySession):
            from webkitcorepy import PartialProxy

            with PartialProxy(
                ['bugs.webkit.org'],
                http='http://proxy.webkit.org:80',
                https='https://proxy.webkit.org:443',
            ):
                import requests
                requests.get('https://bugs.webkit.org')
                self.assertDictEqual(DummySession.PROXIES, dict(
                    http='http://proxy.webkit.org:80',
                    https='https://proxy.webkit.org:443',
                ))

                requests.get('https://commits.webkit.org')
                self.assertIsNone(DummySession.PROXIES)
