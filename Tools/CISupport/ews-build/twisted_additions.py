# Copyright (c) 2014 Peter Ruibal. All rights reserved.
# Copyright (C) 2022 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
# 3.  Neither the name of the copyright holder nor the names of its contributors may
#     be used to endorse or promote products derived from this software without
#     specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED THE COPYRIGHT HOLDERS AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import calendar
import datetime
import json
import os
import re
import twisted

from twisted.internet import defer, error, interfaces, protocol, reactor, task
from twisted.web.client import Agent
from twisted.web.http_headers import Headers
from twisted.web import iweb, http, _newclient

from zope.interface import implementer


@implementer(interfaces.IReactorTCP, interfaces.IReactorSSL)
class HTTPProxyConnector(object):
    """Helper to wrap reactor connection API (TCP, SSL) via a CONNECT proxy."""

    def __init__(self, proxy_host, proxy_port,
                 reactor=reactor):
        self.proxy_host = proxy_host
        self.proxy_port = proxy_port
        self.reactor = reactor

    def seconds(self):
        return self.reactor.seconds()

    def callLater(self, delay, callable, *args, **kw):
        return self.reactor.callLater(delay, callable, *args, **kw)

    def listenTCP(port, factory, backlog=50, interface=''):
        raise error.CannotListenError('Cannot BIND via HTTP proxies')

    def connectTCP(self, host, port, factory, timeout=30, bindAddress=None):
        f = HTTPProxiedClientFactory(factory, host, port)
        self.reactor.connectTCP(self.proxy_host,
                                self.proxy_port,
                                f, timeout, bindAddress)

    def listenSSL(self, port, factory, contextFactory, backlog=50, interface=''):
        raise error.CannotListenError('Cannot BIND via HTTP proxies')

    def connectSSL(self, host, port, factory, contextFactory, timeout=30,
                   bindAddress=None):
        from twisted.protocols import tls
        tlsFactory = tls.TLSMemoryBIOFactory(contextFactory, True, factory)
        return self.connectTCP(host, port, tlsFactory, timeout, bindAddress)


class HTTPProxiedClientFactory(protocol.ClientFactory):
    """ClientFactory wrapper that triggers an HTTP proxy CONNECT on connect"""
    def __init__(self, delegate, dst_host, dst_port):
        self.delegate = delegate
        self.dst_host = dst_host
        self.dst_port = dst_port

    def startedConnecting(self, connector):
        return self.delegate.startedConnecting(connector)

    def buildProtocol(self, addr):
        p = HTTPConnectTunneler(self.dst_host, self.dst_port, addr)
        p.factory = self
        return p

    def clientConnectionFailed(self, connector, reason):
        return self.delegate.clientConnectionFailed(connector, reason)

    def clientConnectionLost(self, connector, reason):
        return self.delegate.clientConnectionLost(connector, reason)


class HTTPConnectTunneler(protocol.Protocol):
    """Protocol that wraps transport with CONNECT proxy handshake on connect
    `factory` MUST be assigned in order to use this Protocol, and the value
    *must* have a `delegate` attribute to trigger wrapped, post-connect,
    factory (creation) methods.
    """
    http = None
    otherConn = None

    def __init__(self, host, port, orig_addr):
        self.host = host
        self.port = port
        self.orig_addr = orig_addr

    def connectionMade(self):
        self.http = HTTPConnectSetup(self.host, self.port)
        self.http.parent = self
        self.http.makeConnection(self.transport)

    def connectionLost(self, reason):
        if self.otherConn is not None:
            self.otherConn.connectionLost(reason)
        if self.http is not None:
            self.http.connectionLost(reason)

    def proxyConnected(self):
        # TODO: Bail if `self.factory` is unassigned or
        # does not have a `delegate`
        self.otherConn = self.factory.delegate.buildProtocol(self.orig_addr)
        self.otherConn.makeConnection(self.transport)

        # Get any pending data from the http buf and forward it to otherConn
        buf = self.http.clearLineBuffer()
        if buf:
            self.otherConn.dataReceived(buf)

    def dataReceived(self, data):
        if self.otherConn is not None:
            return self.otherConn.dataReceived(data)
        elif self.http is not None:
            return self.http.dataReceived(data)
        else:
            raise Exception('No handler for received data... :(')


class HTTPConnectSetup(http.HTTPClient):
    """HTTPClient protocol to send a CONNECT message for proxies.
    `parent` MUST be assigned to an HTTPConnectTunneler instance, or have a
    `proxyConnected` method that will be invoked post-CONNECT (http request)
    """

    def __init__(self, host, port):
        self.host = host
        self.port = port

    def connectionMade(self):
        self.sendCommand(b'CONNECT', f'{self.host}:{self.port}'.encode('utf-8'))
        self.endHeaders()

    def handleStatus(self, version, status, message):
        if status != b'200':
            raise error.ConnectError(f'Unexpected status on CONNECT: {status}')

    def handleEndHeaders(self):
        # TODO: Make sure parent is assigned, and has a proxyConnected callback
        self.parent.proxyConnected()

    def handleResponse(self, body):
        pass


class TwistedAdditions(object):
    PROXY_RE = re.compile(r'https?://(?P<host>.+):(?P<port>\d+)')

    class Response(object):
        def __init__(self, status_code, content=None, url=None, headers=None):
            self.status_code = status_code
            self.content = content or b''
            self.url = url
            self.headers = headers or {}

        @property
        def text(self):
            return self.content.decode('utf-8')

        def json(self):
            return json.loads(self.text)

    @implementer(iweb.IBodyProducer)
    class JSONProducer(object):
        @classmethod
        def serialize(cls, obj):
            if isinstance(obj, datetime.datetime):
                return int(calendar.timegm(obj.timetuple()))
            raise TypeError("Type %s not serializable" % type(obj))

        def __init__(self, data):
            try:
                self.body = json.dumps(data, default=self.serialize).encode('utf-8')
            except TypeError:
                self.body = ''
            self.length = len(self.body)

        def startProducing(self, consumer):
            if self.body:
                consumer.write(self.body)
            return defer.succeed(None)

        def pauseProducing(self):
            pass

        def stopProducing(self):
            pass

    class Printer(protocol.Protocol):
        def __init__(self, finished):
            self.finished = finished
            self.data = b''

        def dataReceived(self, bytes):
            self.data += bytes

        def connectionLost(self, reason):
            self.finished.callback(self.data)

    @classmethod
    @defer.inlineCallbacks
    def request(cls, url, type=None, params=None, headers=None, logger=None, timeout=10, json=None):
        logger = logger or (lambda _: None)
        typ = type or b'GET'

        if not url:
            logger('No URL provided\n')
            defer.returnValue(None)
        hostname = '/'.join(url.split('/', 3)[:3])
        if params:
            url = '{}?{}'.format(url, '&'.join([f'{key}={value}' for key, value in params.items()]))

        headers = headers or {}
        if 'User-Agent' not in headers:
            headers['User-Agent'] = ['python-twisted/{}'.format(twisted.__version__)]

        try:
            proxy = os.getenv('http_proxy') or os.getenv('https_proxy') or os.getenv('HTTP_PROXY') or os.getenv('HTTPS_PROXY')
            match = cls.PROXY_RE.match(proxy) if proxy else None
            if match:
                proxy = HTTPProxyConnector(match.group('host'), int(match.group('port')))
                agent = Agent(proxy, connectTimeout=timeout)
            else:
                agent = Agent(reactor, connectTimeout=timeout)

            body = cls.JSONProducer(json) if json else None
            response = yield agent.request(typ, url.encode('utf-8'), Headers(headers), body)
            finished = defer.Deferred()
            response.deliverBody(cls.Printer(finished))
            data = yield finished

            headers = {}
            for key, values in response.headers.getAllRawHeaders():
                headers[key.decode('utf-8')] = [value.decode('utf-8') for value in values]

            defer.returnValue(cls.Response(
                status_code=response.code,
                headers=headers,
                content=data,
                url=url,
            ))
            return

        except error.ConnectError as e:
            logger(f'Failed to connect to {hostname}\n')
        except _newclient.ResponseFailed:
            logger(f'No response from {hostname}\n')
        except ValueError as e:
            logger(f'{e}\n')
        except Exception as e:
            logger(f'Unknown exception when making request:\n{e}\n')

        defer.returnValue(None)
