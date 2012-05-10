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


"""This file provides classes and helper functions for parsing/building frames
of the WebSocket protocol (RFC 6455).

Specification:
http://tools.ietf.org/html/rfc6455
"""


from collections import deque
import os
import struct

from mod_pywebsocket import common
from mod_pywebsocket import util
from mod_pywebsocket._stream_base import BadOperationException
from mod_pywebsocket._stream_base import ConnectionTerminatedException
from mod_pywebsocket._stream_base import InvalidFrameException
from mod_pywebsocket._stream_base import InvalidUTF8Exception
from mod_pywebsocket._stream_base import StreamBase
from mod_pywebsocket._stream_base import UnsupportedFrameException


_NOOP_MASKER = util.NoopMasker()


class Frame(object):

    def __init__(self, fin=1, rsv1=0, rsv2=0, rsv3=0,
                 opcode=None, payload=''):
        self.fin = fin
        self.rsv1 = rsv1
        self.rsv2 = rsv2
        self.rsv3 = rsv3
        self.opcode = opcode
        self.payload = payload


# Helper functions made public to be used for writing unittests for WebSocket
# clients.


def create_length_header(length, mask):
    """Creates a length header.

    Args:
        length: Frame length. Must be less than 2^63.
        mask: Mask bit. Must be boolean.

    Raises:
        ValueError: when bad data is given.
    """

    if mask:
        mask_bit = 1 << 7
    else:
        mask_bit = 0

    if length < 0:
        raise ValueError('length must be non negative integer')
    elif length <= 125:
        return chr(mask_bit | length)
    elif length < (1 << 16):
        return chr(mask_bit | 126) + struct.pack('!H', length)
    elif length < (1 << 63):
        return chr(mask_bit | 127) + struct.pack('!Q', length)
    else:
        raise ValueError('Payload is too big for one frame')


def create_header(opcode, payload_length, fin, rsv1, rsv2, rsv3, mask):
    """Creates a frame header.

    Raises:
        Exception: when bad data is given.
    """

    if opcode < 0 or 0xf < opcode:
        raise ValueError('Opcode out of range')

    if payload_length < 0 or (1 << 63) <= payload_length:
        raise ValueError('payload_length out of range')

    if (fin | rsv1 | rsv2 | rsv3) & ~1:
        raise ValueError('FIN bit and Reserved bit parameter must be 0 or 1')

    header = ''

    first_byte = ((fin << 7)
                  | (rsv1 << 6) | (rsv2 << 5) | (rsv3 << 4)
                  | opcode)
    header += chr(first_byte)
    header += create_length_header(payload_length, mask)

    return header


def _build_frame(header, body, mask):
    if not mask:
        return header + body

    masking_nonce = os.urandom(4)
    masker = util.RepeatedXorMasker(masking_nonce)

    return header + masking_nonce + masker.mask(body)


def _filter_and_format_frame_object(frame, mask, frame_filters):
    for frame_filter in frame_filters:
        frame_filter.filter(frame)

    header = create_header(
        frame.opcode, len(frame.payload), frame.fin,
        frame.rsv1, frame.rsv2, frame.rsv3, mask)
    return _build_frame(header, frame.payload, mask)


def create_binary_frame(
    message, opcode=common.OPCODE_BINARY, fin=1, mask=False, frame_filters=[]):
    """Creates a simple binary frame with no extension, reserved bit."""

    frame = Frame(fin=fin, opcode=opcode, payload=message)
    return _filter_and_format_frame_object(frame, mask, frame_filters)


def create_text_frame(
    message, opcode=common.OPCODE_TEXT, fin=1, mask=False, frame_filters=[]):
    """Creates a simple text frame with no extension, reserved bit."""

    encoded_message = message.encode('utf-8')
    return create_binary_frame(encoded_message, opcode, fin, mask,
                               frame_filters)


class FragmentedFrameBuilder(object):
    """A stateful class to send a message as fragments."""

    def __init__(self, mask, frame_filters=[]):
        """Constructs an instance."""

        self._mask = mask
        self._frame_filters = frame_filters

        self._started = False

        # Hold opcode of the first frame in messages to verify types of other
        # frames in the message are all the same.
        self._opcode = common.OPCODE_TEXT

    def build(self, message, end, binary):
        if binary:
            frame_type = common.OPCODE_BINARY
        else:
            frame_type = common.OPCODE_TEXT
        if self._started:
            if self._opcode != frame_type:
                raise ValueError('Message types are different in frames for '
                                 'the same message')
            opcode = common.OPCODE_CONTINUATION
        else:
            opcode = frame_type
            self._opcode = frame_type

        if end:
            self._started = False
            fin = 1
        else:
            self._started = True
            fin = 0

        if binary:
            return create_binary_frame(
                message, opcode, fin, self._mask, self._frame_filters)
        else:
            return create_text_frame(
                message, opcode, fin, self._mask, self._frame_filters)


def _create_control_frame(opcode, body, mask, frame_filters):
    frame = Frame(opcode=opcode, payload=body)

    return _filter_and_format_frame_object(frame, mask, frame_filters)


def create_ping_frame(body, mask=False, frame_filters=[]):
    return _create_control_frame(common.OPCODE_PING, body, mask, frame_filters)


def create_pong_frame(body, mask=False, frame_filters=[]):
    return _create_control_frame(common.OPCODE_PONG, body, mask, frame_filters)


def create_close_frame(body, mask=False, frame_filters=[]):
    return _create_control_frame(
        common.OPCODE_CLOSE, body, mask, frame_filters)


class StreamOptions(object):
    """Holds option values to configure Stream objects."""

    def __init__(self):
        """Constructs StreamOptions."""

        # Enables deflate-stream extension.
        self.deflate_stream = False

        # Filters applied to frames.
        self.outgoing_frame_filters = []
        self.incoming_frame_filters = []

        self.mask_send = False
        self.unmask_receive = True


class Stream(StreamBase):
    """A class for parsing/building frames of the WebSocket protocol
    (RFC 6455).
    """

    def __init__(self, request, options):
        """Constructs an instance.

        Args:
            request: mod_python request.
        """

        StreamBase.__init__(self, request)

        self._logger = util.get_class_logger(self)

        self._options = options

        if self._options.deflate_stream:
            self._logger.debug('Setup filter for deflate-stream')
            self._request = util.DeflateRequest(self._request)

        self._request.client_terminated = False
        self._request.server_terminated = False

        # Holds body of received fragments.
        self._received_fragments = []
        # Holds the opcode of the first fragment.
        self._original_opcode = None

        self._writer = FragmentedFrameBuilder(
            self._options.mask_send, self._options.outgoing_frame_filters)

        self._ping_queue = deque()

    def _receive_frame(self):
        """Receives a frame and return data in the frame as a tuple containing
        each header field and payload separately.

        Raises:
            ConnectionTerminatedException: when read returns empty
                string.
            InvalidFrameException: when the frame contains invalid data.
        """

        received = self.receive_bytes(2)

        first_byte = ord(received[0])
        fin = (first_byte >> 7) & 1
        rsv1 = (first_byte >> 6) & 1
        rsv2 = (first_byte >> 5) & 1
        rsv3 = (first_byte >> 4) & 1
        opcode = first_byte & 0xf

        second_byte = ord(received[1])
        mask = (second_byte >> 7) & 1
        payload_length = second_byte & 0x7f

        if (mask == 1) != self._options.unmask_receive:
            raise InvalidFrameException(
                'Mask bit on the received frame did\'nt match masking '
                'configuration for received frames')

        # The Hybi-13 and later specs disallow putting a value in 0x0-0xFFFF
        # into the 8-octet extended payload length field (or 0x0-0xFD in
        # 2-octet field).
        valid_length_encoding = True
        length_encoding_bytes = 1
        if payload_length == 127:
            extended_payload_length = self.receive_bytes(8)
            payload_length = struct.unpack(
                '!Q', extended_payload_length)[0]
            if payload_length > 0x7FFFFFFFFFFFFFFF:
                raise InvalidFrameException(
                    'Extended payload length >= 2^63')
            if self._request.ws_version >= 13 and payload_length < 0x10000:
                valid_length_encoding = False
                length_encoding_bytes = 8
        elif payload_length == 126:
            extended_payload_length = self.receive_bytes(2)
            payload_length = struct.unpack(
                '!H', extended_payload_length)[0]
            if self._request.ws_version >= 13 and payload_length < 126:
                valid_length_encoding = False
                length_encoding_bytes = 2

        if not valid_length_encoding:
            self._logger.warning(
                'Payload length is not encoded using the minimal number of '
                'bytes (%d is encoded using %d bytes)',
                payload_length,
                length_encoding_bytes)

        if mask == 1:
            masking_nonce = self.receive_bytes(4)
            masker = util.RepeatedXorMasker(masking_nonce)
        else:
            masker = _NOOP_MASKER

        bytes = masker.mask(self.receive_bytes(payload_length))

        return opcode, bytes, fin, rsv1, rsv2, rsv3

    def _receive_frame_as_frame_object(self):
        opcode, bytes, fin, rsv1, rsv2, rsv3 = self._receive_frame()

        return Frame(fin=fin, rsv1=rsv1, rsv2=rsv2, rsv3=rsv3,
                     opcode=opcode, payload=bytes)

    def send_message(self, message, end=True, binary=False):
        """Send message.

        Args:
            message: text in unicode or binary in str to send.
            binary: send message as binary frame.

        Raises:
            BadOperationException: when called on a server-terminated
                connection or called with inconsistent message type or binary
                parameter.
        """

        if self._request.server_terminated:
            raise BadOperationException(
                'Requested send_message after sending out a closing handshake')

        if binary and isinstance(message, unicode):
            raise BadOperationException(
                'Message for binary frame must be instance of str')

        try:
            self._write(self._writer.build(message, end, binary))
        except ValueError, e:
            raise BadOperationException(e)

    def receive_message(self):
        """Receive a WebSocket frame and return its payload as a text in
        unicode or a binary in str.

        Returns:
            payload data of the frame
            - as unicode instance if received text frame
            - as str instance if received binary frame
            or None iff received closing handshake.
        Raises:
            BadOperationException: when called on a client-terminated
                connection.
            ConnectionTerminatedException: when read returns empty
                string.
            InvalidFrameException: when the frame contains invalid
                data.
            UnsupportedFrameException: when the received frame has
                flags, opcode we cannot handle. You can ignore this
                exception and continue receiving the next frame.
        """

        if self._request.client_terminated:
            raise BadOperationException(
                'Requested receive_message after receiving a closing '
                'handshake')

        while True:
            # mp_conn.read will block if no bytes are available.
            # Timeout is controlled by TimeOut directive of Apache.

            frame = self._receive_frame_as_frame_object()

            for frame_filter in self._options.incoming_frame_filters:
                frame_filter.filter(frame)

            if frame.rsv1 or frame.rsv2 or frame.rsv3:
                raise UnsupportedFrameException(
                    'Unsupported flag is set (rsv = %d%d%d)' %
                    (frame.rsv1, frame.rsv2, frame.rsv3))

            if frame.opcode == common.OPCODE_CONTINUATION:
                if not self._received_fragments:
                    if frame.fin:
                        raise InvalidFrameException(
                            'Received a termination frame but fragmentation '
                            'not started')
                    else:
                        raise InvalidFrameException(
                            'Received an intermediate frame but '
                            'fragmentation not started')

                if frame.fin:
                    # End of fragmentation frame
                    self._received_fragments.append(frame.payload)
                    message = ''.join(self._received_fragments)
                    self._received_fragments = []
                else:
                    # Intermediate frame
                    self._received_fragments.append(frame.payload)
                    continue
            else:
                if self._received_fragments:
                    if frame.fin:
                        raise InvalidFrameException(
                            'Received an unfragmented frame without '
                            'terminating existing fragmentation')
                    else:
                        raise InvalidFrameException(
                            'New fragmentation started without terminating '
                            'existing fragmentation')

                if frame.fin:
                    # Unfragmented frame

                    if (common.is_control_opcode(frame.opcode) and
                        len(frame.payload) > 125):
                        raise InvalidFrameException(
                            'Application data size of control frames must be '
                            '125 bytes or less')

                    self._original_opcode = frame.opcode
                    message = frame.payload
                else:
                    # Start of fragmentation frame

                    if common.is_control_opcode(frame.opcode):
                        raise InvalidFrameException(
                            'Control frames must not be fragmented')

                    self._original_opcode = frame.opcode
                    self._received_fragments.append(frame.payload)
                    continue

            if self._original_opcode == common.OPCODE_TEXT:
                # The WebSocket protocol section 4.4 specifies that invalid
                # characters must be replaced with U+fffd REPLACEMENT
                # CHARACTER.
                try:
                    return message.decode('utf-8')
                except UnicodeDecodeError, e:
                    raise InvalidUTF8Exception(e)
            elif self._original_opcode == common.OPCODE_BINARY:
                return message
            elif self._original_opcode == common.OPCODE_CLOSE:
                self._request.client_terminated = True

                # Status code is optional. We can have status reason only if we
                # have status code. Status reason can be empty string. So,
                # allowed cases are
                # - no application data: no code no reason
                # - 2 octet of application data: has code but no reason
                # - 3 or more octet of application data: both code and reason
                if len(message) == 1:
                    raise InvalidFrameException(
                        'If a close frame has status code, the length of '
                        'status code must be 2 octet')
                elif len(message) >= 2:
                    self._request.ws_close_code = struct.unpack(
                        '!H', message[0:2])[0]
                    self._request.ws_close_reason = message[2:].decode(
                        'utf-8', 'replace')
                    self._logger.debug(
                        'Received close frame (code=%d, reason=%r)',
                        self._request.ws_close_code,
                        self._request.ws_close_reason)

                # Drain junk data after the close frame if necessary.
                self._drain_received_data()

                if self._request.server_terminated:
                    self._logger.debug(
                        'Received ack for server-initiated closing '
                        'handshake')
                    return None

                self._logger.debug(
                    'Received client-initiated closing handshake')

                code = common.STATUS_NORMAL_CLOSURE
                reason = ''
                if hasattr(self._request, '_dispatcher'):
                    dispatcher = self._request._dispatcher
                    code, reason = dispatcher.passive_closing_handshake(
                        self._request)
                self._send_closing_handshake(code, reason)
                self._logger.debug(
                    'Sent ack for client-initiated closing handshake')
                return None
            elif self._original_opcode == common.OPCODE_PING:
                try:
                    handler = self._request.on_ping_handler
                    if handler:
                        handler(self._request, message)
                        continue
                except AttributeError, e:
                    pass
                self._send_pong(message)
            elif self._original_opcode == common.OPCODE_PONG:
                # TODO(tyoshino): Add ping timeout handling.

                inflight_pings = deque()

                while True:
                    try:
                        expected_body = self._ping_queue.popleft()
                        if expected_body == message:
                            # inflight_pings contains pings ignored by the
                            # other peer. Just forget them.
                            self._logger.debug(
                                'Ping %r is acked (%d pings were ignored)',
                                expected_body, len(inflight_pings))
                            break
                        else:
                            inflight_pings.append(expected_body)
                    except IndexError, e:
                        # The received pong was unsolicited pong. Keep the
                        # ping queue as is.
                        self._ping_queue = inflight_pings
                        self._logger.debug('Received a unsolicited pong')
                        break

                try:
                    handler = self._request.on_pong_handler
                    if handler:
                        handler(self._request, message)
                        continue
                except AttributeError, e:
                    pass

                continue
            else:
                raise UnsupportedFrameException(
                    'Opcode %d is not supported' % self._original_opcode)

    def _send_closing_handshake(self, code, reason):
        if code >= (1 << 16) or code < 0:
            raise BadOperationException('Status code is out of range')

        encoded_reason = reason.encode('utf-8')
        if len(encoded_reason) + 2 > 125:
            raise BadOperationException(
                'Application data size of close frames must be 125 bytes or '
                'less')

        frame = create_close_frame(
            struct.pack('!H', code) + encoded_reason,
            self._options.mask_send,
            self._options.outgoing_frame_filters)

        self._request.server_terminated = True

        self._write(frame)

    def close_connection(self, code=common.STATUS_NORMAL_CLOSURE, reason=''):
        """Closes a WebSocket connection."""

        if self._request.server_terminated:
            self._logger.debug(
                'Requested close_connection but server is already terminated')
            return

        self._send_closing_handshake(code, reason)
        self._logger.debug('Sent server-initiated closing handshake')

        if (code == common.STATUS_GOING_AWAY or
            code == common.STATUS_PROTOCOL_ERROR):
            # It doesn't make sense to wait for a close frame if the reason is
            # protocol error or that the server is going away. For some of
            # other reasons, it might not make sense to wait for a close frame,
            # but it's not clear, yet.
            return

        # TODO(ukai): 2. wait until the /client terminated/ flag has been set,
        # or until a server-defined timeout expires.
        #
        # For now, we expect receiving closing handshake right after sending
        # out closing handshake.
        message = self.receive_message()
        if message is not None:
            raise ConnectionTerminatedException(
                'Didn\'t receive valid ack for closing handshake')
        # TODO: 3. close the WebSocket connection.
        # note: mod_python Connection (mp_conn) doesn't have close method.

    def send_ping(self, body=''):
        if len(body) > 125:
            raise ValueError(
                'Application data size of control frames must be 125 bytes or '
                'less')
        frame = create_ping_frame(
            body,
            self._options.mask_send,
            self._options.outgoing_frame_filters)
        self._write(frame)

        self._ping_queue.append(body)

    def _send_pong(self, body):
        if len(body) > 125:
            raise ValueError(
                'Application data size of control frames must be 125 bytes or '
                'less')
        frame = create_pong_frame(
            body,
            self._options.mask_send,
            self._options.outgoing_frame_filters)
        self._write(frame)

    def _drain_received_data(self):
        """Drains unread data in the receive buffer to avoid sending out TCP
        RST packet. This is because when deflate-stream is enabled, some
        DEFLATE block for flushing data may follow a close frame. If any data
        remains in the receive buffer of a socket when the socket is closed,
        it sends out TCP RST packet to the other peer.

        Since mod_python's mp_conn object doesn't support non-blocking read,
        we perform this only when pywebsocket is running in standalone mode.
        """

        # If self._options.deflate_stream is true, self._request is
        # DeflateRequest, so we can get wrapped request object by
        # self._request._request.
        #
        # Only _StandaloneRequest has _drain_received_data method.
        if (self._options.deflate_stream and
            ('_drain_received_data' in dir(self._request._request))):
            self._request._request._drain_received_data()


# vi:sts=4 sw=4 et
