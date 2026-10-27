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

from webkitcorepy.requests_utils import describe_request_error, requests


class RequestsUtilsTest(unittest.TestCase):
    def test_timeout(self):
        self.assertEqual(describe_request_error(requests.exceptions.ReadTimeout('Read timed out.')), 'timed out waiting for a response')
        self.assertEqual(describe_request_error(requests.exceptions.ConnectTimeout('Connect timed out.')), 'timed out waiting for a response')

    def test_connection_error(self):
        self.assertEqual(describe_request_error(requests.exceptions.ConnectionError('Max retries exceeded')), 'could not connect')

    def test_other_error(self):
        self.assertEqual(
            describe_request_error(requests.exceptions.InvalidURL('Invalid URL /rest/bug/1?login=myuser&password=mysecret')),
            'Invalid URL /rest/bug/1?login=<REDACTED>&password=<REDACTED>',
        )
