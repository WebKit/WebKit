# Copyright 2012, Google Inc.
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


# Constants indicating WebSocket protocol version.
VERSION_HIXIE75 = -1
VERSION_HYBI00 = 0
VERSION_HYBI01 = 1
VERSION_HYBI02 = 2
VERSION_HYBI03 = 2
VERSION_HYBI04 = 4
VERSION_HYBI05 = 5
VERSION_HYBI06 = 6
VERSION_HYBI07 = 7
VERSION_HYBI08 = 8
VERSION_HYBI09 = 8
VERSION_HYBI10 = 8
VERSION_HYBI11 = 8
VERSION_HYBI12 = 8
VERSION_HYBI13 = 13
VERSION_HYBI14 = 13
VERSION_HYBI15 = 13
VERSION_HYBI16 = 13
VERSION_HYBI17 = 13

# Constants indicating WebSocket protocol latest version.
VERSION_HYBI_LATEST = VERSION_HYBI13

# Port numbers
DEFAULT_WEB_SOCKET_PORT = 80
DEFAULT_WEB_SOCKET_SECURE_PORT = 443

# Schemes
WEB_SOCKET_SCHEME = 'ws'
WEB_SOCKET_SECURE_SCHEME = 'wss'

# Frame opcodes defined in the spec.
OPCODE_CONTINUATION = 0x0
OPCODE_TEXT = 0x1
OPCODE_BINARY = 0x2
OPCODE_CLOSE = 0x8
OPCODE_PING = 0x9
OPCODE_PONG = 0xa

# UUIDs used by HyBi 04 and later opening handshake and frame masking.
WEBSOCKET_ACCEPT_UUID = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11'

# Opening handshake header names and expected values.
UPGRADE_HEADER = 'Upgrade'
WEBSOCKET_UPGRADE_TYPE = 'websocket'
WEBSOCKET_UPGRADE_TYPE_HIXIE75 = 'WebSocket'
CONNECTION_HEADER = 'Connection'
UPGRADE_CONNECTION_TYPE = 'Upgrade'
HOST_HEADER = 'Host'
ORIGIN_HEADER = 'Origin'
SEC_WEBSOCKET_ORIGIN_HEADER = 'Sec-WebSocket-Origin'
SEC_WEBSOCKET_KEY_HEADER = 'Sec-WebSocket-Key'
SEC_WEBSOCKET_ACCEPT_HEADER = 'Sec-WebSocket-Accept'
SEC_WEBSOCKET_VERSION_HEADER = 'Sec-WebSocket-Version'
SEC_WEBSOCKET_PROTOCOL_HEADER = 'Sec-WebSocket-Protocol'
SEC_WEBSOCKET_EXTENSIONS_HEADER = 'Sec-WebSocket-Extensions'
SEC_WEBSOCKET_DRAFT_HEADER = 'Sec-WebSocket-Draft'
SEC_WEBSOCKET_KEY1_HEADER = 'Sec-WebSocket-Key1'
SEC_WEBSOCKET_KEY2_HEADER = 'Sec-WebSocket-Key2'
SEC_WEBSOCKET_LOCATION_HEADER = 'Sec-WebSocket-Location'

# Extensions
DEFLATE_STREAM_EXTENSION = 'deflate-stream'
DEFLATE_FRAME_EXTENSION = 'deflate-frame'
X_WEBKIT_DEFLATE_FRAME_EXTENSION = 'x-webkit-deflate-frame'

# Status codes
# Code STATUS_NO_STATUS_RECEIVED, STATUS_ABNORMAL_CLOSURE, and
# STATUS_TLS_HANDSHAKE are pseudo codes to indicate specific error cases.
# Could not be used for codes in actual closing frames.
# Application level errors must use codes in the range
# STATUS_USER_REGISTERED_BASE to STATUS_USER_PRIVATE_MAX. The codes in the
# range STATUS_USER_REGISTERED_BASE to STATUS_USER_REGISTERED_MAX are managed
# by IANA. Usually application must define user protocol level errors in the
# range STATUS_USER_PRIVATE_BASE to STATUS_USER_PRIVATE_MAX.
STATUS_NORMAL_CLOSURE = 1000
STATUS_GOING_AWAY = 1001
STATUS_PROTOCOL_ERROR = 1002
STATUS_UNSUPPORTED_DATA = 1003
STATUS_NO_STATUS_RECEIVED = 1005
STATUS_ABNORMAL_CLOSURE = 1006
STATUS_INVALID_FRAME_PAYLOAD_DATA = 1007
STATUS_POLICY_VIOLATION = 1008
STATUS_MESSAGE_TOO_BIG = 1009
STATUS_MANDATORY_EXTENSION = 1010
STATUS_INTERNAL_SERVER_ERROR = 1011
STATUS_TLS_HANDSHAKE = 1015
STATUS_USER_REGISTERED_BASE = 3000
STATUS_USER_REGISTERED_MAX = 3999
STATUS_USER_PRIVATE_BASE = 4000
STATUS_USER_PRIVATE_MAX = 4999
# Following definitions are aliases to keep compatibility. Applications must
# not use these obsoleted definitions anymore.
STATUS_NORMAL = STATUS_NORMAL_CLOSURE
STATUS_UNSUPPORTED = STATUS_UNSUPPORTED_DATA
STATUS_CODE_NOT_AVAILABLE = STATUS_NO_STATUS_RECEIVED
STATUS_ABNORMAL_CLOSE = STATUS_ABNORMAL_CLOSURE
STATUS_INVALID_FRAME_PAYLOAD = STATUS_INVALID_FRAME_PAYLOAD_DATA
STATUS_MANDATORY_EXT = STATUS_MANDATORY_EXTENSION

# HTTP status codes
HTTP_STATUS_BAD_REQUEST = 400
HTTP_STATUS_FORBIDDEN = 403
HTTP_STATUS_NOT_FOUND = 404


def is_control_opcode(opcode):
    return (opcode >> 3) == 1


class ExtensionParameter(object):
    """Holds information about an extension which is exchanged on extension
    negotiation in opening handshake.
    """

    def __init__(self, name):
        self._name = name
        # TODO(tyoshino): Change the data structure to more efficient one such
        # as dict when the spec changes to say like
        # - Parameter names must be unique
        # - The order of parameters is not significant
        self._parameters = []

    def name(self):
        return self._name

    def add_parameter(self, name, value):
        self._parameters.append((name, value))

    def get_parameters(self):
        return self._parameters

    def get_parameter_names(self):
        return [name for name, unused_value in self._parameters]

    def has_parameter(self, name):
        for param_name, param_value in self._parameters:
            if param_name == name:
                return True
        return False

    def get_parameter_value(self, name):
        for param_name, param_value in self._parameters:
            if param_name == name:
                return param_value


# vi:sts=4 sw=4 et
