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
from unittest import mock

# Puts the WPT webdriver client on the path.
from webkitpy.webdriver_tests.webdriver_w3c_executor import WebDriverW3CExecutor

from webdriver.transport import HTTPWireProtocol  # noqa: E402


class ReportingLaunchesTest(unittest.TestCase):

    def setUp(self):
        self.sent = []
        self.reports = []
        self.error = None

        def send(protocol, method, uri, body=None, **kwargs):
            self.sent.append((method, uri))
            self.reports.append('sent')
            if self.error:
                raise self.error
            return 'response'

        patcher = mock.patch.object(HTTPWireProtocol, 'send', send)
        patcher.start()
        self.addCleanup(patcher.stop)
        self.send = send
        self.protocol = HTTPWireProtocol('127.0.0.1', 4444)

    def test_new_session(self):
        with WebDriverW3CExecutor._reporting_launches(self.reports.append):
            self.assertEqual(self.protocol.send('POST', 'session', {'capabilities': {}}), 'response')
            self.protocol.send('POST', '/session/')
        self.assertEqual(self.sent, [('POST', 'session'), ('POST', '/session/')])
        self.assertEqual(self.reports, [('launch', 'start'), 'sent', ('launch', 'end')] * 2)

    def test_other_commands(self):
        with WebDriverW3CExecutor._reporting_launches(self.reports.append):
            self.protocol.send('GET', 'session/1234/url')
            self.protocol.send('POST', 'session/1234/url', {'url': 'about:blank'})
            self.protocol.send('DELETE', 'session/1234')
            self.protocol.send('GET', 'status')
        self.assertEqual(len(self.sent), 4)
        self.assertEqual(self.reports, ['sent'] * 4)

    def test_failed_new_session(self):
        # Like when the driver goes away. The launch ends anyway.
        self.error = ConnectionError()
        with WebDriverW3CExecutor._reporting_launches(self.reports.append):
            with self.assertRaises(ConnectionError):
                self.protocol.send('POST', 'session')
        self.assertEqual(self.reports, [('launch', 'start'), 'sent', ('launch', 'end')])

    def test_restores_send(self):
        with WebDriverW3CExecutor._reporting_launches(self.reports.append):
            self.assertIsNot(HTTPWireProtocol.send, self.send)
        self.assertIs(HTTPWireProtocol.send, self.send)
