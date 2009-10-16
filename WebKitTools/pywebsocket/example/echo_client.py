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


"""Web Socket Echo client.

This is an example Web Socket client that talks with echo_wsh.py.
This may be useful for checking mod_pywebsocket installation.

Note:
This code is far from robust, e.g., we cut corners in handshake.
"""


import codecs
from optparse import OptionParser
import socket
import sys


_DEFAULT_PORT = 80
_DEFAULT_SECURE_PORT = 443
_UNDEFINED_PORT = -1

_UPGRADE_HEADER = 'Upgrade: WebSocket\r\n'
_CONNECTION_HEADER = 'Connection: Upgrade\r\n'
_EXPECTED_RESPONSE = (
        'HTTP/1.1 101 Web Socket Protocol Handshake\r\n' +
        _UPGRADE_HEADER +
        _CONNECTION_HEADER)


def _method_line(resource):
    return 'GET %s HTTP/1.1\r\n' % resource


def _origin_header(origin):
    return 'Origin: %s\r\n' % origin


class _TLSSocket(object):
    """Wrapper for a TLS connection."""

    def __init__(self, raw_socket):
        self._ssl = socket.ssl(raw_socket)

    def send(self, bytes):
        return self._ssl.write(bytes)

    def recv(self, size=-1):
        return self._ssl.read(size)

    def close(self):
        # Nothing to do.
        pass


class EchoClient(object):
    """Web Socket echo client."""

    def __init__(self, options):
        self._options = options
        self._socket = None

    def run(self):
        """Run the client.

        Shake hands and then repeat sending message and receiving its echo.
        """
        self._socket = socket.socket()
        try:
            self._socket.connect((self._options.server_host,
                                  self._options.server_port))
            if self._options.use_tls:
                self._socket = _TLSSocket(self._socket)
            self._handshake()
            for line in self._options.message.split(','):
                frame = '\x00' + line.encode('utf-8') + '\xff'
                self._socket.send(frame)
                if self._options.verbose:
                    print 'Send: %s' % line
                received = self._socket.recv(len(frame))
                if received != frame:
                    raise Exception('Incorrect echo: %r' % received)
                if self._options.verbose:
                    print 'Recv: %s' % received[1:-1].decode('utf-8')
        finally:
            self._socket.close()

    def _handshake(self):
        self._socket.send(_method_line(self._options.resource))
        self._socket.send(_UPGRADE_HEADER)
        self._socket.send(_CONNECTION_HEADER)
        self._socket.send(self._format_host_header())
        self._socket.send(_origin_header(self._options.origin))
        self._socket.send('\r\n')

        for expected_char in _EXPECTED_RESPONSE:
            received = self._socket.recv(1)[0]
            if expected_char != received:
                raise Exception('Handshake failure')
        # We cut corners and skip other headers.
        self._skip_headers()

    def _skip_headers(self):
        terminator = '\r\n\r\n'
        pos = 0
        while pos < len(terminator):
            received = self._socket.recv(1)[0]
            if received == terminator[pos]:
                pos += 1
            elif received == terminator[0]:
                pos = 1
            else:
                pos = 0

    def _format_host_header(self):
        host = 'Host: ' + self._options.server_host
        if ((not self._options.use_tls and
             self._options.server_port != _DEFAULT_PORT) or
            (self._options.use_tls and
             self._options.server_port != _DEFAULT_SECURE_PORT)):
            host += ':' + str(self._options.server_port)
        host += '\r\n'
        return host


def main():
    sys.stdout = codecs.getwriter('utf-8')(sys.stdout)

    parser = OptionParser()
    parser.add_option('-s', '--server_host', dest='server_host', type='string',
                      default='localhost', help='server host')
    parser.add_option('-p', '--server_port', dest='server_port', type='int',
                      default=_UNDEFINED_PORT, help='server port')
    parser.add_option('-o', '--origin', dest='origin', type='string',
                      default='http://localhost/', help='origin')
    parser.add_option('-r', '--resource', dest='resource', type='string',
                      default='/echo', help='resource path')
    parser.add_option('-m', '--message', dest='message', type='string',
                      help='comma-separated messages to send')
    parser.add_option('-q', '--quiet', dest='verbose', action='store_false',
                      default=True, help='suppress messages')
    parser.add_option('-t', '--tls', dest='use_tls', action='store_true',
                      default=False, help='use TLS (wss://)')
    (options, unused_args) = parser.parse_args()

    # Default port number depends on whether TLS is used.
    if options.server_port == _UNDEFINED_PORT:
        if options.use_tls:
            options.server_port = _DEFAULT_SECURE_PORT
        else:
            options.server_port = _DEFAULT_PORT

    # optparse doesn't seem to handle non-ascii default values.
    # Set default message here.
    if not options.message:
        options.message = u'Hello,\u65e5\u672c'   # "Japan" in Japanese

    EchoClient(options).run()


if __name__ == '__main__':
    main()


# vi:sts=4 sw=4 et
