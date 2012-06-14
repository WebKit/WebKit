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


from mod_pywebsocket import common
from mod_pywebsocket import util
from mod_pywebsocket.http_header_util import quote_if_necessary


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


def _parse_compression_method(data):
    """Parses the value of "method" extension parameter."""

    return common.parse_extensions(data, allow_quoted_string=True)


def _create_accepted_method_desc(method_name, method_params):
    """Creates accepted-method-desc from given method name and parameters"""

    extension = common.ExtensionParameter(method_name)
    for name, value in method_params:
        extension.add_parameter(name, value)
    return common.format_extension(extension)


class PerFrameCompressionExtensionProcessor(ExtensionProcessorInterface):
    """WebSocket Per-frame compression extension processor."""

    _METHOD_PARAM = 'method'
    _DEFLATE_METHOD = 'deflate'

    def __init__(self, request):
        self._logger = util.get_class_logger(self)
        self._request = request
        self._compression_method_name = None
        self._compression_processor = None

    def _lookup_compression_processor(self, method_desc):
        if method_desc.name() == self._DEFLATE_METHOD:
            return DeflateFrameExtensionProcessor(method_desc)
        return None

    def _get_compression_processor_response(self):
        """Looks up the compression processor based on the self._request and
           returns the compression processor's response.
        """

        method_list = self._request.get_parameter_value(self._METHOD_PARAM)
        if method_list is None:
            return None
        methods = _parse_compression_method(method_list)
        if methods is None:
            return None
        comression_processor = None
        # The current implementation tries only the first method that matches
        # supported algorithm. Following methods aren't tried even if the
        # first one is rejected.
        # TODO(bashi): Need to clarify this behavior.
        for method_desc in methods:
            compression_processor = self._lookup_compression_processor(
                method_desc)
            if compression_processor is not None:
                self._compression_method_name = method_desc.name()
                break
        if compression_processor is None:
            return None
        processor_response = compression_processor.get_extension_response()
        if processor_response is None:
            return None
        self._compression_processor = compression_processor
        return processor_response

    def get_extension_response(self):
        processor_response = self._get_compression_processor_response()
        if processor_response is None:
            return None

        response = common.ExtensionParameter(self._request.name())
        accepted_method_desc = _create_accepted_method_desc(
                                   self._compression_method_name,
                                   processor_response.get_parameters())
        response.add_parameter(self._METHOD_PARAM, accepted_method_desc)
        self._logger.debug(
            'Enable %s extension (method: %s)' %
            (self._request.name(), self._compression_method_name))
        return response

    def setup_stream_options(self, stream_options):
        if self._compression_processor is None:
            return
        self._compression_processor.setup_stream_options(stream_options)

    def get_compression_processor(self):
        return self._compression_processor


_available_processors[common.PERFRAME_COMPRESSION_EXTENSION] = (
    PerFrameCompressionExtensionProcessor)


def get_extension_processor(extension_request):
    global _available_processors
    processor_class = _available_processors.get(extension_request.name())
    if processor_class is None:
        return None
    return processor_class(extension_request)


# vi:sts=4 sw=4 et
