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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
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

from twisted.internet import base, endpoints, error, protocol
from twisted.internet.testing import StringTransport
from twisted.python.failure import Failure

from .twisted_additions import HTTPProxiedClientFactory


class FakeTransport(object):
    def __init__(self):
        self.lost = False
        self.failed = None

    def loseConnection(self):
        self.lost = True

    def failIfNotConnected(self, err):
        self.failed = err


class FakeConnector(base.BaseConnector):
    def _makeTransport(self):
        return FakeTransport()

    def getDestination(self):
        return None


class TestHTTPProxiedClientFactory(unittest.TestCase):
    def cancel_attempt(self, connector_state):
        connector = FakeConnector(protocol.ClientFactory(), timeout=None, reactor=None)
        transport = FakeTransport()
        connector.transport = transport
        connector.state = connector_state

        wrapping_factory = endpoints._WrappingFactory(protocol.Factory())
        HTTPProxiedClientFactory(wrapping_factory, 'results.webkit.org', 443).startedConnecting(connector)
        failures = []
        wrapping_factory._onConnection.addErrback(failures.append)
        wrapping_factory._onConnection.cancel()
        return failures, transport

    def test_cancel_while_connecting_to_proxy(self):
        failures, transport = self.cancel_attempt('connecting')
        self.assertEqual(len(failures), 1)
        self.assertTrue(failures[0].check(error.ConnectingCancelledError))
        self.assertIsInstance(transport.failed, error.UserError)
        self.assertFalse(transport.lost)

    def test_cancel_while_tunnel_is_being_established(self):
        failures, transport = self.cancel_attempt('connected')
        self.assertEqual(len(failures), 1)
        self.assertTrue(failures[0].check(error.ConnectingCancelledError))
        self.assertTrue(transport.lost)

    def open_tunnel(self, proxy_reply):
        connector = FakeConnector(protocol.ClientFactory(), timeout=None, reactor=None)
        built = []
        wrapped_factory = protocol.Factory.forProtocol(protocol.Protocol)
        wrapped_factory.buildProtocol = lambda addr: built.append(addr) or protocol.Protocol()
        wrapping_factory = endpoints._WrappingFactory(wrapped_factory)
        proxied_factory = HTTPProxiedClientFactory(wrapping_factory, 'results.webkit.org', 443)
        proxied_factory.startedConnecting(connector)
        results = []
        wrapping_factory._onConnection.addBoth(results.append)

        tunnel = proxied_factory.buildProtocol(None)
        transport = StringTransport()
        tunnel.makeConnection(transport)
        self.assertEqual(transport.value(), b'CONNECT results.webkit.org:443 HTTP/1.0\r\n\r\n')
        tunnel.dataReceived(proxy_reply)
        return proxied_factory, connector, transport, results, built

    def test_proxy_refuses_tunnel(self):
        proxied_factory, connector, transport, results, built = self.open_tunnel(b'HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\n\r\n')
        self.assertTrue(transport.disconnecting)
        self.assertEqual(results, [])
        proxied_factory.clientConnectionLost(connector, Failure(error.ConnectionDone()))
        self.assertEqual(len(results), 1)
        self.assertTrue(results[0].check(error.ConnectError))
        self.assertIn('proxy answered CONNECT results.webkit.org:443 with 503 Service Unavailable', str(results[0].value))
        self.assertEqual(built, [])

    def test_proxy_drops_connection_before_tunnel(self):
        proxied_factory, connector, transport, results, built = self.open_tunnel(b'')
        proxied_factory.clientConnectionLost(connector, Failure(error.ConnectionLost()))
        self.assertEqual(len(results), 1)
        self.assertTrue(results[0].check(error.ConnectionLost))

    def test_proxy_establishes_tunnel(self):
        proxied_factory, connector, transport, results, built = self.open_tunnel(b'HTTP/1.1 200 Connection established\r\n\r\n')
        self.assertFalse(transport.disconnecting)
        self.assertEqual(len(built), 1)
        self.assertEqual(len(results), 1)
        self.assertIsInstance(results[0], protocol.Protocol)
        proxied_factory.clientConnectionLost(connector, Failure(error.ConnectionDone()))
        self.assertEqual(len(results), 1)
