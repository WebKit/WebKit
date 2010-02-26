#!/usr/bin/env python
#
# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


"""Tests for handshake module."""


import unittest

import config  # This must be imported before mod_pywebsocket.
from mod_pywebsocket import handshake

import mock


_GOOD_REQUEST = (
    80,
    '/demo',
    {
        'Upgrade':'WebSocket',
        'Connection':'Upgrade',
        'Host':'example.com',
        'Origin':'http://example.com',
        'WebSocket-Protocol':'sample',
    }
)

_GOOD_RESPONSE_DEFAULT_PORT = (
    'HTTP/1.1 101 Web Socket Protocol Handshake\r\n'
    'Upgrade: WebSocket\r\n'
    'Connection: Upgrade\r\n'
    'WebSocket-Origin: http://example.com\r\n'
    'WebSocket-Location: ws://example.com/demo\r\n'
    'WebSocket-Protocol: sample\r\n'
    '\r\n')

_GOOD_RESPONSE_SECURE = (
    'HTTP/1.1 101 Web Socket Protocol Handshake\r\n'
    'Upgrade: WebSocket\r\n'
    'Connection: Upgrade\r\n'
    'WebSocket-Origin: http://example.com\r\n'
    'WebSocket-Location: wss://example.com/demo\r\n'
    'WebSocket-Protocol: sample\r\n'
    '\r\n')

_GOOD_REQUEST_NONDEFAULT_PORT = (
    8081,
    '/demo',
    {
        'Upgrade':'WebSocket',
        'Connection':'Upgrade',
        'Host':'example.com:8081',
        'Origin':'http://example.com',
        'WebSocket-Protocol':'sample',
    }
)

_GOOD_RESPONSE_NONDEFAULT_PORT = (
    'HTTP/1.1 101 Web Socket Protocol Handshake\r\n'
    'Upgrade: WebSocket\r\n'
    'Connection: Upgrade\r\n'
    'WebSocket-Origin: http://example.com\r\n'
    'WebSocket-Location: ws://example.com:8081/demo\r\n'
    'WebSocket-Protocol: sample\r\n'
    '\r\n')

_GOOD_RESPONSE_SECURE_NONDEF = (
    'HTTP/1.1 101 Web Socket Protocol Handshake\r\n'
    'Upgrade: WebSocket\r\n'
    'Connection: Upgrade\r\n'
    'WebSocket-Origin: http://example.com\r\n'
    'WebSocket-Location: wss://example.com:8081/demo\r\n'
    'WebSocket-Protocol: sample\r\n'
    '\r\n')

_GOOD_REQUEST_NO_PROTOCOL = (
    80,
    '/demo',
    {
        'Upgrade':'WebSocket',
        'Connection':'Upgrade',
        'Host':'example.com',
        'Origin':'http://example.com',
    }
)

_GOOD_RESPONSE_NO_PROTOCOL = (
    'HTTP/1.1 101 Web Socket Protocol Handshake\r\n'
    'Upgrade: WebSocket\r\n'
    'Connection: Upgrade\r\n'
    'WebSocket-Origin: http://example.com\r\n'
    'WebSocket-Location: ws://example.com/demo\r\n'
    '\r\n')

_GOOD_REQUEST_WITH_OPTIONAL_HEADERS = (
    80,
    '/demo',
    {
        'Upgrade':'WebSocket',
        'Connection':'Upgrade',
        'Host':'example.com',
        'Origin':'http://example.com',
        'WebSocket-Protocol':'sample',
        'AKey':'AValue',
        'EmptyValue':'',
    }
)

_BAD_REQUESTS = (
    (  # HTTP request
        80,
        '/demo',
        {
            'Host':'www.google.com',
            'User-Agent':'Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10.5;'
                         ' en-US; rv:1.9.1.3) Gecko/20090824 Firefox/3.5.3'
                         ' GTB6 GTBA',
            'Accept':'text/html,application/xhtml+xml,application/xml;q=0.9,'
                     '*/*;q=0.8',
            'Accept-Language':'en-us,en;q=0.5',
            'Accept-Encoding':'gzip,deflate',
            'Accept-Charset':'ISO-8859-1,utf-8;q=0.7,*;q=0.7',
            'Keep-Alive':'300',
            'Connection':'keep-alive',
        }
    ),
    (  # Missing Upgrade
        80,
        '/demo',
        {
            'Connection':'Upgrade',
            'Host':'example.com',
            'Origin':'http://example.com',
            'WebSocket-Protocol':'sample',
        }
    ),
    (  # Wrong Upgrade
        80,
        '/demo',
        {
            'Upgrade':'NonWebSocket',
            'Connection':'Upgrade',
            'Host':'example.com',
            'Origin':'http://example.com',
            'WebSocket-Protocol':'sample',
        }
    ),
    (  # Empty WebSocket-Protocol
        80,
        '/demo',
        {
            'Upgrade':'WebSocket',
            'Connection':'Upgrade',
            'Host':'example.com',
            'Origin':'http://example.com',
            'WebSocket-Protocol':'',
        }
    ),
    (  # Wrong port number format
        80,
        '/demo',
        {
            'Upgrade':'WebSocket',
            'Connection':'Upgrade',
            'Host':'example.com:0x50',
            'Origin':'http://example.com',
            'WebSocket-Protocol':'sample',
        }
    ),
    (  # Header/connection port mismatch
        8080,
        '/demo',
        {
            'Upgrade':'WebSocket',
            'Connection':'Upgrade',
            'Host':'example.com',
            'Origin':'http://example.com',
            'WebSocket-Protocol':'sample',
        }
    ),
    (  # Illegal WebSocket-Protocol
        80,
        '/demo',
        {
            'Upgrade':'WebSocket',
            'Connection':'Upgrade',
            'Host':'example.com',
            'Origin':'http://example.com',
            'WebSocket-Protocol':'illegal\x09protocol',
        }
    ),
)

_STRICTLY_GOOD_REQUESTS = (
    (
        'GET /demo HTTP/1.1\r\n',
        'Upgrade: WebSocket\r\n',
        'Connection: Upgrade\r\n',
        'Host: example.com\r\n',
        'Origin: http://example.com\r\n',
        '\r\n',
    ),
    (  # WebSocket-Protocol
        'GET /demo HTTP/1.1\r\n',
        'Upgrade: WebSocket\r\n',
        'Connection: Upgrade\r\n',
        'Host: example.com\r\n',
        'Origin: http://example.com\r\n',
        'WebSocket-Protocol: sample\r\n',
        '\r\n',
    ),
    (  # WebSocket-Protocol and Cookie
        'GET /demo HTTP/1.1\r\n',
        'Upgrade: WebSocket\r\n',
        'Connection: Upgrade\r\n',
        'Host: example.com\r\n',
        'Origin: http://example.com\r\n',
        'WebSocket-Protocol: sample\r\n',
        'Cookie: xyz\r\n'
        '\r\n',
    ),
    (  # Cookie
        'GET /demo HTTP/1.1\r\n',
        'Upgrade: WebSocket\r\n',
        'Connection: Upgrade\r\n',
        'Host: example.com\r\n',
        'Origin: http://example.com\r\n',
        'Cookie: abc/xyz\r\n'
        'Cookie2: $Version=1\r\n'
        'Cookie: abc\r\n'
        '\r\n',
    ),
    (
        'GET / HTTP/1.1\r\n',
        'Upgrade: WebSocket\r\n',
        'Connection: Upgrade\r\n',
        'Host: example.com\r\n',
        'Origin: http://example.com\r\n',
        '\r\n',
    ),
)

_NOT_STRICTLY_GOOD_REQUESTS = (
    (  # Extra space after GET
        'GET  /demo HTTP/1.1\r\n',
        'Upgrade: WebSocket\r\n',
        'Connection: Upgrade\r\n',
        'Host: example.com\r\n',
        'Origin: http://example.com\r\n',
        '\r\n',
    ),
    (  # Resource name doesn't stat with '/'
        'GET demo HTTP/1.1\r\n',
        'Upgrade: WebSocket\r\n',
        'Connection: Upgrade\r\n',
        'Host: example.com\r\n',
        'Origin: http://example.com\r\n',
        '\r\n',
    ),
    (  # No space after :
        'GET /demo HTTP/1.1\r\n',
        'Upgrade:WebSocket\r\n',
        'Connection: Upgrade\r\n',
        'Host: example.com\r\n',
        'Origin: http://example.com\r\n',
        '\r\n',
    ),
    (  # Lower case Upgrade header
        'GET /demo HTTP/1.1\r\n',
        'upgrade: WebSocket\r\n',
        'Connection: Upgrade\r\n',
        'Host: example.com\r\n',
        'Origin: http://example.com\r\n',
        '\r\n',
    ),
    (  # Connection comes before Upgrade
        'GET /demo HTTP/1.1\r\n',
        'Connection: Upgrade\r\n',
        'Upgrade: WebSocket\r\n',
        'Host: example.com\r\n',
        'Origin: http://example.com\r\n',
        '\r\n',
    ),
    (  # Origin comes before Host
        'GET /demo HTTP/1.1\r\n',
        'Upgrade: WebSocket\r\n',
        'Connection: Upgrade\r\n',
        'Origin: http://example.com\r\n',
        'Host: example.com\r\n',
        '\r\n',
    ),
    (  # Host continued to the next line
        'GET /demo HTTP/1.1\r\n',
        'Upgrade: WebSocket\r\n',
        'Connection: Upgrade\r\n',
        'Host: example\r\n',
        ' .com\r\n',
        'Origin: http://example.com\r\n',
        '\r\n',
    ),
    ( # Cookie comes before WebSocket-Protocol
        'GET /demo HTTP/1.1\r\n',
        'Upgrade: WebSocket\r\n',
        'Connection: Upgrade\r\n',
        'Host: example.com\r\n',
        'Origin: http://example.com\r\n',
        'Cookie: xyz\r\n'
        'WebSocket-Protocol: sample\r\n',
        '\r\n',
    ),
    (  # Unknown header
        'GET /demo HTTP/1.1\r\n',
        'Upgrade: WebSocket\r\n',
        'Connection: Upgrade\r\n',
        'Host: example.com\r\n',
        'Origin: http://example.com\r\n',
        'Content-Type: text/html\r\n'
        '\r\n',
    ),
    (  # Cookie with continuation lines
        'GET /demo HTTP/1.1\r\n',
        'Upgrade: WebSocket\r\n',
        'Connection: Upgrade\r\n',
        'Host: example.com\r\n',
        'Origin: http://example.com\r\n',
        'Cookie: xyz\r\n',
        ' abc\r\n',
        ' defg\r\n',
        '\r\n',
    ),
    (  # Wrong-case cookie
        'GET /demo HTTP/1.1\r\n',
        'Upgrade: WebSocket\r\n',
        'Connection: Upgrade\r\n',
        'Host: example.com\r\n',
        'Origin: http://example.com\r\n',
        'cookie: abc/xyz\r\n'
        '\r\n',
    ),
    (  # Cookie, no space after colon
        'GET /demo HTTP/1.1\r\n',
        'Upgrade: WebSocket\r\n',
        'Connection: Upgrade\r\n',
        'Host: example.com\r\n',
        'Origin: http://example.com\r\n',
        'Cookie:abc/xyz\r\n'
        '\r\n',
    ),
)


def _create_request(request_def):
    conn = mock.MockConn('')
    conn.local_addr = ('0.0.0.0', request_def[0])
    return mock.MockRequest(
            uri=request_def[1],
            headers_in=request_def[2],
            connection=conn)


def _create_get_memorized_lines(lines):
    def get_memorized_lines():
        return lines
    return get_memorized_lines


def _create_requests_with_lines(request_lines_set):
    requests = []
    for lines in request_lines_set:
        request = _create_request(_GOOD_REQUEST)
        request.connection.get_memorized_lines = _create_get_memorized_lines(
                lines)
        requests.append(request)
    return requests


class HandshakerTest(unittest.TestCase):
    def test_validate_protocol(self):
        handshake._validate_protocol('sample')  # should succeed.
        handshake._validate_protocol('Sample')  # should succeed.
        handshake._validate_protocol('sample\x20protocol')  # should succeed.
        handshake._validate_protocol('sample\x7eprotocol')  # should succeed.
        self.assertRaises(handshake.HandshakeError,
                          handshake._validate_protocol,
                          '')
        self.assertRaises(handshake.HandshakeError,
                          handshake._validate_protocol,
                          'sample\x19protocol')
        self.assertRaises(handshake.HandshakeError,
                          handshake._validate_protocol,
                          'sample\x7fprotocol')
        self.assertRaises(handshake.HandshakeError,
                          handshake._validate_protocol,
                          # "Japan" in Japanese
                          u'\u65e5\u672c')

    def test_good_request_default_port(self):
        request = _create_request(_GOOD_REQUEST)
        handshaker = handshake.Handshaker(request,
                                          mock.MockDispatcher())
        handshaker.do_handshake()
        self.assertEqual(_GOOD_RESPONSE_DEFAULT_PORT,
                         request.connection.written_data())
        self.assertEqual('/demo', request.ws_resource)
        self.assertEqual('http://example.com', request.ws_origin)
        self.assertEqual('ws://example.com/demo', request.ws_location)
        self.assertEqual('sample', request.ws_protocol)

    def test_good_request_secure_default_port(self):
        request = _create_request(_GOOD_REQUEST)
        request.connection.local_addr = ('0.0.0.0', 443)
        request.is_https_ = True
        handshaker = handshake.Handshaker(request,
                                          mock.MockDispatcher())
        handshaker.do_handshake()
        self.assertEqual(_GOOD_RESPONSE_SECURE,
                         request.connection.written_data())
        self.assertEqual('sample', request.ws_protocol)

    def test_good_request_nondefault_port(self):
        request = _create_request(_GOOD_REQUEST_NONDEFAULT_PORT)
        handshaker = handshake.Handshaker(request,
                                          mock.MockDispatcher())
        handshaker.do_handshake()
        self.assertEqual(_GOOD_RESPONSE_NONDEFAULT_PORT,
                         request.connection.written_data())
        self.assertEqual('sample', request.ws_protocol)

    def test_good_request_secure_non_default_port(self):
        request = _create_request(_GOOD_REQUEST_NONDEFAULT_PORT)
        request.is_https_ = True
        handshaker = handshake.Handshaker(request,
                                          mock.MockDispatcher())
        handshaker.do_handshake()
        self.assertEqual(_GOOD_RESPONSE_SECURE_NONDEF,
                         request.connection.written_data())
        self.assertEqual('sample', request.ws_protocol)

    def test_good_request_default_no_protocol(self):
        request = _create_request(_GOOD_REQUEST_NO_PROTOCOL)
        handshaker = handshake.Handshaker(request,
                                          mock.MockDispatcher())
        handshaker.do_handshake()
        self.assertEqual(_GOOD_RESPONSE_NO_PROTOCOL,
                         request.connection.written_data())
        self.assertEqual(None, request.ws_protocol)

    def test_good_request_optional_headers(self):
        request = _create_request(_GOOD_REQUEST_WITH_OPTIONAL_HEADERS)
        handshaker = handshake.Handshaker(request,
                                          mock.MockDispatcher())
        handshaker.do_handshake()
        self.assertEqual('AValue',
                         request.headers_in['AKey'])
        self.assertEqual('',
                         request.headers_in['EmptyValue'])

    def test_bad_requests(self):
        for request in map(_create_request, _BAD_REQUESTS):
            handshaker = handshake.Handshaker(request,
                                              mock.MockDispatcher())
            self.assertRaises(handshake.HandshakeError, handshaker.do_handshake)

    def test_strictly_good_requests(self):
        for request in _create_requests_with_lines(_STRICTLY_GOOD_REQUESTS):
            strict_handshaker = handshake.Handshaker(request,
                                                     mock.MockDispatcher(),
                                                     True)
            strict_handshaker.do_handshake()

    def test_not_strictly_good_requests(self):
        for request in _create_requests_with_lines(_NOT_STRICTLY_GOOD_REQUESTS):
            strict_handshaker = handshake.Handshaker(request,
                                                     mock.MockDispatcher(),
                                                     True)
            self.assertRaises(handshake.HandshakeError,
                              strict_handshaker.do_handshake)



if __name__ == '__main__':
    unittest.main()


# vi:sts=4 sw=4 et
