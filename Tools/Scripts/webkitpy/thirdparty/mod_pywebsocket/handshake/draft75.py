# Copyright 2011, Google Inc.
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


"""WebSocket handshaking defined in draft-hixie-thewebsocketprotocol-75."""


# Note: request.connection.write is used in this module, even though mod_python
# document says that it should be used only in connection handlers.
# Unfortunately, we have no other options. For example, request.write is not
# suitable because it doesn't allow direct raw bytes writing.


import logging
import re

from mod_pywebsocket import common
from mod_pywebsocket.stream import StreamHixie75
from mod_pywebsocket import util
from mod_pywebsocket.handshake._base import HandshakeException
from mod_pywebsocket.handshake._base import build_location
from mod_pywebsocket.handshake._base import validate_subprotocol


_MANDATORY_HEADERS = [
    # key, expected value or None
    ['Upgrade', 'WebSocket'],
    ['Connection', 'Upgrade'],
    ['Host', None],
    ['Origin', None],
]

_FIRST_FIVE_LINES = map(re.compile, [
    r'^GET /[\S]* HTTP/1.1\r\n$',
    r'^Upgrade: WebSocket\r\n$',
    r'^Connection: Upgrade\r\n$',
    r'^Host: [\S]+\r\n$',
    r'^Origin: [\S]+\r\n$',
])

_SIXTH_AND_LATER = re.compile(
    r'^'
    r'(WebSocket-Protocol: [\x20-\x7e]+\r\n)?'
    r'(Cookie: [^\r]*\r\n)*'
    r'(Cookie2: [^\r]*\r\n)?'
    r'(Cookie: [^\r]*\r\n)*'
    r'\r\n')


class Handshaker(object):
    """This class performs WebSocket handshake."""

    def __init__(self, request, dispatcher, strict=False):
        """Construct an instance.

        Args:
            request: mod_python request.
            dispatcher: Dispatcher (dispatch.Dispatcher).
            strict: Strictly check handshake request. Default: False.
                If True, request.connection must provide get_memorized_lines
                method.

        Handshaker will add attributes such as ws_resource in performing
        handshake.
        """

        self._logger = util.get_class_logger(self)

        self._request = request
        self._dispatcher = dispatcher
        self._strict = strict

    def do_handshake(self):
        """Perform WebSocket Handshake.

        On _request, we set
            ws_resource, ws_origin, ws_location, ws_protocol
            ws_challenge_md5: WebSocket handshake information.
            ws_stream: Frame generation/parsing class.
            ws_version: Protocol version.
        """

        self._check_header_lines()
        self._set_resource()
        self._set_origin()
        self._set_location()
        self._set_subprotocol()
        self._set_protocol_version()

        self._dispatcher.do_extra_handshake(self._request)

        self._send_handshake()

        self._logger.debug('Sent opening handshake response')

    def _set_resource(self):
        self._request.ws_resource = self._request.uri

    def _set_origin(self):
        self._request.ws_origin = self._request.headers_in['Origin']

    def _set_location(self):
        self._request.ws_location = build_location(self._request)

    def _set_subprotocol(self):
        subprotocol = self._request.headers_in.get('WebSocket-Protocol')
        if subprotocol is not None:
            validate_subprotocol(subprotocol, hixie=True)
        self._request.ws_protocol = subprotocol

    def _set_protocol_version(self):
        self._logger.debug('IETF Hixie 75 protocol')
        self._request.ws_version = common.VERSION_HIXIE75
        self._request.ws_stream = StreamHixie75(self._request)

    def _sendall(self, data):
        self._request.connection.write(data)

    def _send_handshake(self):
        self._sendall('HTTP/1.1 101 Web Socket Protocol Handshake\r\n')
        self._sendall('Upgrade: WebSocket\r\n')
        self._sendall('Connection: Upgrade\r\n')
        self._sendall('WebSocket-Origin: %s\r\n' % self._request.ws_origin)
        self._sendall('WebSocket-Location: %s\r\n' % self._request.ws_location)
        if self._request.ws_protocol:
            self._sendall(
                'WebSocket-Protocol: %s\r\n' % self._request.ws_protocol)
        self._sendall('\r\n')

    def _check_header_lines(self):
        for key, expected_value in _MANDATORY_HEADERS:
            actual_value = self._request.headers_in.get(key)
            if not actual_value:
                raise HandshakeException('Header %s is not defined' % key)
            if expected_value:
                if actual_value != expected_value:
                    raise HandshakeException(
                        'Expected %r for header %s but found %r' %
                        (expected_value, key, actual_value))
        if self._strict:
            try:
                lines = self._request.connection.get_memorized_lines()
            except AttributeError, e:
                raise AttributeError(
                    'Strict handshake is specified but the connection '
                    'doesn\'t provide get_memorized_lines()')
            self._check_first_lines(lines)

    def _check_first_lines(self, lines):
        if len(lines) < len(_FIRST_FIVE_LINES):
            raise HandshakeException('Too few header lines: %d' % len(lines))
        for line, regexp in zip(lines, _FIRST_FIVE_LINES):
            if not regexp.search(line):
                raise HandshakeException(
                    'Unexpected header: %r doesn\'t match %r'
                    % (line, regexp.pattern))
        sixth_and_later = ''.join(lines[5:])
        if not _SIXTH_AND_LATER.search(sixth_and_later):
            raise HandshakeException(
                'Unexpected header: %r doesn\'t match %r'
                % (sixth_and_later, _SIXTH_AND_LATER.pattern))


# vi:sts=4 sw=4 et
