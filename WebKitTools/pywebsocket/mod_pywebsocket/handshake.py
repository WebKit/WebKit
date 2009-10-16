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


"""Web Socket handshaking.

Note: request.connection.write/read are used in this module, even though
mod_python document says that they should be used only in connection handlers.
Unfortunately, we have no other options. For example, request.write/read are
not suitable because they don't allow direct raw bytes writing/reading.
"""


import re


_DEFAULT_WEB_SOCKET_PORT = 80
_DEFAULT_WEB_SOCKET_SECURE_PORT = 443
_WEB_SOCKET_SCHEME = 'ws'
_WEB_SOCKET_SECURE_SCHEME = 'wss'

_METHOD_LINE = re.compile(r'^GET ([^ ]+) HTTP/1.1\r\n$')

_MANDATORY_HEADERS = [
    # key, expected value or None
    ['Upgrade', 'WebSocket'],
    ['Connection', 'Upgrade'],
    ['Host', None],
    ['Origin', None],
]


def _default_port(is_secure):
    if is_secure:
        return _DEFAULT_WEB_SOCKET_SECURE_PORT
    else:
        return _DEFAULT_WEB_SOCKET_PORT


class HandshakeError(Exception):
    """Exception in Web Socket Handshake."""

    pass


def _validate_protocol(protocol):
    """Validate WebSocket-Protocol string."""

    if not protocol:
        raise HandshakeError('Invalid WebSocket-Protocol: empty')
    for c in protocol:
        if not 0x21 <= ord(c) <= 0x7e:
            raise HandshakeError('Illegal character in protocol: %r' % c)


class Handshaker(object):
    """This class performs Web Socket handshake."""

    def __init__(self, request, dispatcher):
        """Construct an instance.

        Args:
            request: mod_python request.
            dispatcher: Dispatcher (dispatch.Dispatcher).

        Handshaker will add attributes such as ws_resource in performing
        handshake.
        """

        self._request = request
        self._dispatcher = dispatcher

    def do_handshake(self):
        """Perform Web Socket Handshake."""

        self._check_header_lines()
        self._set_resource()
        self._set_origin()
        self._set_location()
        self._set_protocol()
        self._dispatcher.do_extra_handshake(self._request)
        self._send_handshake()

    def _set_resource(self):
        self._request.ws_resource = self._request.uri

    def _set_origin(self):
        self._request.ws_origin = self._request.headers_in['Origin']

    def _set_location(self):
        location_parts = []
        if self._request.is_https():
            location_parts.append(_WEB_SOCKET_SECURE_SCHEME)
        else:
            location_parts.append(_WEB_SOCKET_SCHEME)
        location_parts.append('://')
        host, port = self._parse_host_header()
        connection_port = self._request.connection.local_addr[1]
        if port != connection_port:
            raise HandshakeError('Header/connection port mismatch: %d/%d' %
                                 (port, connection_port))
        location_parts.append(host)
        if (port != _default_port(self._request.is_https())):
            location_parts.append(':')
            location_parts.append(str(port))
        location_parts.append(self._request.uri)
        self._request.ws_location = ''.join(location_parts)

    def _parse_host_header(self):
        fields = self._request.headers_in['Host'].split(':', 1)
        if len(fields) == 1:
            return fields[0], _default_port(self._request.is_https())
        try:
            return fields[0], int(fields[1])
        except ValueError, e:
            raise HandshakeError('Invalid port number format: %r' % e)

    def _set_protocol(self):
        protocol = self._request.headers_in.get('WebSocket-Protocol')
        if protocol is not None:
            _validate_protocol(protocol)
        self._request.ws_protocol = protocol

    def _send_handshake(self):
        self._request.connection.write(
                'HTTP/1.1 101 Web Socket Protocol Handshake\r\n')
        self._request.connection.write('Upgrade: WebSocket\r\n')
        self._request.connection.write('Connection: Upgrade\r\n')
        self._request.connection.write('WebSocket-Origin: ')
        self._request.connection.write(self._request.ws_origin)
        self._request.connection.write('\r\n')
        self._request.connection.write('WebSocket-Location: ')
        self._request.connection.write(self._request.ws_location)
        self._request.connection.write('\r\n')
        if self._request.ws_protocol:
            self._request.connection.write('WebSocket-Protocol: ')
            self._request.connection.write(self._request.ws_protocol)
            self._request.connection.write('\r\n')
        self._request.connection.write('\r\n')

    def _check_header_lines(self):
        for key, expected_value in _MANDATORY_HEADERS:
            actual_value = self._request.headers_in.get(key)
            if not actual_value:
                raise HandshakeError('Header %s is not defined' % key)
            if expected_value:
                if actual_value != expected_value:
                    raise HandshakeError('Illegal value for header %s: %s' %
                                         (key, actual_value))


# vi:sts=4 sw=4 et
