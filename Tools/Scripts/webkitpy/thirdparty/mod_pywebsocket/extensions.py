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


from mod_pywebsocket import common
from mod_pywebsocket import util


_available_processors = {}


class ExtensionProcessorInterface(object):

    def get_extension_response(self):
        return None

    def setup_stream_options(self, stream_options):
        pass


class DeflateStreamExtensionProcessor(ExtensionProcessorInterface):
    """WebSocket DEFLATE stream extension processor."""

    def __init__(self, request):
        self._logger = util.get_class_logger(self)

        self._request = request

    def get_extension_response(self):
        if len(self._request.get_parameter_names()) != 0:
            return None

        self._logger.debug(
            'Enable %s extension', common.DEFLATE_STREAM_EXTENSION)

        return common.ExtensionParameter(common.DEFLATE_STREAM_EXTENSION)

    def setup_stream_options(self, stream_options):
        stream_options.deflate_stream = True


_available_processors[common.DEFLATE_STREAM_EXTENSION] = (
    DeflateStreamExtensionProcessor)


class DeflateFrameExtensionProcessor(ExtensionProcessorInterface):
    """WebSocket Per-frame DEFLATE extension processor."""

    _WINDOW_BITS_PARAM = 'max_window_bits'
    _NO_CONTEXT_TAKEOVER_PARAM = 'no_context_takeover'

    def __init__(self, request):
        self._logger = util.get_class_logger(self)

        self._request = request

        self._response_window_bits = None
        self._response_no_context_takeover = False

        # Counters for statistics.

        # Total number of outgoing bytes supplied to this filter.
        self._total_outgoing_payload_bytes = 0
        # Total number of bytes sent to the network after applying this filter.
        self._total_filtered_outgoing_payload_bytes = 0

        # Total number of bytes received from the network.
        self._total_incoming_payload_bytes = 0
        # Total number of incoming bytes obtained after applying this filter.
        self._total_filtered_incoming_payload_bytes = 0

    def get_extension_response(self):
        # Any unknown parameter will be just ignored.

        window_bits = self._request.get_parameter_value(
            self._WINDOW_BITS_PARAM)
        no_context_takeover = self._request.has_parameter(
            self._NO_CONTEXT_TAKEOVER_PARAM)
        if (no_context_takeover and
            self._request.get_parameter_value(
                self._NO_CONTEXT_TAKEOVER_PARAM) is not None):
            return None

        if window_bits is not None:
            try:
                window_bits = int(window_bits)
            except ValueError, e:
                return None
            if window_bits < 8 or window_bits > 15:
                return None

        self._deflater = util._RFC1979Deflater(
            window_bits, no_context_takeover)

        self._inflater = util._RFC1979Inflater()

        self._compress_outgoing = True

        response = common.ExtensionParameter(self._request.name())

        if self._response_window_bits is not None:
            response.add_parameter(
                self._WINDOW_BITS_PARAM, str(self._response_window_bits))
        if self._response_no_context_takeover:
            response.add_parameter(
                self._NO_CONTEXT_TAKEOVER_PARAM, None)

        self._logger.debug(
            'Enable %s extension ('
            'request: window_bits=%s; no_context_takeover=%r, '
            'response: window_wbits=%s; no_context_takeover=%r)' %
            (self._request.name(),
             window_bits,
             no_context_takeover,
             self._response_window_bits,
             self._response_no_context_takeover))

        return response

    def setup_stream_options(self, stream_options):

        class _OutgoingFilter(object):

            def __init__(self, parent):
                self._parent = parent

            def filter(self, frame):
                self._parent._outgoing_filter(frame)

        class _IncomingFilter(object):

            def __init__(self, parent):
                self._parent = parent

            def filter(self, frame):
                self._parent._incoming_filter(frame)

        stream_options.outgoing_frame_filters.append(
            _OutgoingFilter(self))
        stream_options.incoming_frame_filters.insert(
            0, _IncomingFilter(self))

    def set_response_window_bits(self, value):
        self._response_window_bits = value

    def set_response_no_context_takeover(self, value):
        self._response_no_context_takeover = value

    def enable_outgoing_compression(self):
        self._compress_outgoing = True

    def disable_outgoing_compression(self):
        self._compress_outgoing = False

    def _outgoing_filter(self, frame):
        """Transform outgoing frames. This method is called only by
        an _OutgoingFilter instance.
        """

        original_payload_size = len(frame.payload)
        self._total_outgoing_payload_bytes += original_payload_size

        if (not self._compress_outgoing or
            common.is_control_opcode(frame.opcode)):
            self._total_filtered_outgoing_payload_bytes += (
                original_payload_size)
            return

        frame.payload = self._deflater.filter(frame.payload)
        frame.rsv1 = 1

        filtered_payload_size = len(frame.payload)
        self._total_filtered_outgoing_payload_bytes += filtered_payload_size

        # Print inf when ratio is not available.
        ratio = float('inf')
        average_ratio = float('inf')
        if original_payload_size != 0:
            ratio = float(filtered_payload_size) / original_payload_size
        if self._total_outgoing_payload_bytes != 0:
            average_ratio = (
                float(self._total_filtered_outgoing_payload_bytes) /
                self._total_outgoing_payload_bytes)
        self._logger.debug(
            'Outgoing compress ratio: %f (average: %f)' %
            (ratio, average_ratio))

    def _incoming_filter(self, frame):
        """Transform incoming frames. This method is called only by
        an _IncomingFilter instance.
        """

        received_payload_size = len(frame.payload)
        self._total_incoming_payload_bytes += received_payload_size

        if frame.rsv1 != 1 or common.is_control_opcode(frame.opcode):
            self._total_filtered_incoming_payload_bytes += (
                received_payload_size)
            return

        frame.payload = self._inflater.filter(frame.payload)
        frame.rsv1 = 0

        filtered_payload_size = len(frame.payload)
        self._total_filtered_incoming_payload_bytes += filtered_payload_size

        # Print inf when ratio is not available.
        ratio = float('inf')
        average_ratio = float('inf')
        if received_payload_size != 0:
            ratio = float(received_payload_size) / filtered_payload_size
        if self._total_filtered_incoming_payload_bytes != 0:
            average_ratio = (
                float(self._total_incoming_payload_bytes) /
                self._total_filtered_incoming_payload_bytes)
        self._logger.debug(
            'Incoming compress ratio: %f (average: %f)' %
            (ratio, average_ratio))


_available_processors[common.DEFLATE_FRAME_EXTENSION] = (
    DeflateFrameExtensionProcessor)


# Adding vendor-prefixed deflate-frame extension.
# TODO(bashi): Remove this after WebKit stops using vender prefix.
_available_processors[common.X_WEBKIT_DEFLATE_FRAME_EXTENSION] = (
    DeflateFrameExtensionProcessor)


def get_extension_processor(extension_request):
    global _available_processors
    processor_class = _available_processors.get(extension_request.name())
    if processor_class is None:
        return None
    return processor_class(extension_request)


# vi:sts=4 sw=4 et
