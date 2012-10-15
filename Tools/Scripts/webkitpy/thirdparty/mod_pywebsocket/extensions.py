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

    def name(self):
        return None

    def get_extension_response(self):
        return None

    def setup_stream_options(self, stream_options):
        pass


class DeflateStreamExtensionProcessor(ExtensionProcessorInterface):
    """WebSocket DEFLATE stream extension processor.

    Specification:
    Section 9.2.1 in
    http://tools.ietf.org/html/draft-ietf-hybi-thewebsocketprotocol-10
    """

    def __init__(self, request):
        self._logger = util.get_class_logger(self)

        self._request = request

    def name(self):
        return common.DEFLATE_STREAM_EXTENSION

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


def _log_compression_ratio(logger, original_bytes, total_original_bytes,
                           filtered_bytes, total_filtered_bytes):
    # Print inf when ratio is not available.
    ratio = float('inf')
    average_ratio = float('inf')
    if original_bytes != 0:
        ratio = float(filtered_bytes) / original_bytes
    if total_original_bytes != 0:
        average_ratio = (
            float(total_filtered_bytes) / total_original_bytes)
    logger.debug('Outgoing compress ratio: %f (average: %f)' %
        (ratio, average_ratio))


def _log_decompression_ratio(logger, received_bytes, total_received_bytes,
                             filtered_bytes, total_filtered_bytes):
    # Print inf when ratio is not available.
    ratio = float('inf')
    average_ratio = float('inf')
    if received_bytes != 0:
        ratio = float(received_bytes) / filtered_bytes
    if total_filtered_bytes != 0:
        average_ratio = (
            float(total_received_bytes) / total_filtered_bytes)
    logger.debug('Incoming compress ratio: %f (average: %f)' %
        (ratio, average_ratio))


class DeflateFrameExtensionProcessor(ExtensionProcessorInterface):
    """WebSocket Per-frame DEFLATE extension processor.

    Specification:
    http://tools.ietf.org/html/draft-tyoshino-hybi-websocket-perframe-deflate
    """

    _WINDOW_BITS_PARAM = 'max_window_bits'
    _NO_CONTEXT_TAKEOVER_PARAM = 'no_context_takeover'

    def __init__(self, request):
        self._logger = util.get_class_logger(self)

        self._request = request

        self._response_window_bits = None
        self._response_no_context_takeover = False
        self._bfinal = False

        # Counters for statistics.

        # Total number of outgoing bytes supplied to this filter.
        self._total_outgoing_payload_bytes = 0
        # Total number of bytes sent to the network after applying this filter.
        self._total_filtered_outgoing_payload_bytes = 0

        # Total number of bytes received from the network.
        self._total_incoming_payload_bytes = 0
        # Total number of incoming bytes obtained after applying this filter.
        self._total_filtered_incoming_payload_bytes = 0

    def name(self):
        return common.DEFLATE_FRAME_EXTENSION

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

    def set_bfinal(self, value):
        self._bfinal = value

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

        frame.payload = self._deflater.filter(
            frame.payload, bfinal=self._bfinal)
        frame.rsv1 = 1

        filtered_payload_size = len(frame.payload)
        self._total_filtered_outgoing_payload_bytes += filtered_payload_size

        _log_compression_ratio(self._logger, original_payload_size,
                               self._total_outgoing_payload_bytes,
                               filtered_payload_size,
                               self._total_filtered_outgoing_payload_bytes)

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

        _log_decompression_ratio(self._logger, received_payload_size,
                                 self._total_incoming_payload_bytes,
                                 filtered_payload_size,
                                 self._total_filtered_incoming_payload_bytes)


_available_processors[common.DEFLATE_FRAME_EXTENSION] = (
    DeflateFrameExtensionProcessor)


# Adding vendor-prefixed deflate-frame extension.
# TODO(bashi): Remove this after WebKit stops using vendor prefix.
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


class CompressionExtensionProcessorBase(ExtensionProcessorInterface):
    """Base class for Per-frame and Per-message compression extension."""

    _METHOD_PARAM = 'method'

    def __init__(self, request):
        self._logger = util.get_class_logger(self)
        self._request = request
        self._compression_method_name = None
        self._compression_processor = None
        self._compression_processor_hook = None

    def name(self):
        return ''

    def _lookup_compression_processor(self, method_desc):
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

        if self._compression_processor_hook:
            self._compression_processor_hook(compression_processor)

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

    def set_compression_processor_hook(self, hook):
        self._compression_processor_hook = hook

    def get_compression_processor(self):
        return self._compression_processor


class PerFrameCompressionExtensionProcessor(CompressionExtensionProcessorBase):
    """WebSocket Per-frame compression extension processor.

    Specification:
    http://tools.ietf.org/html/draft-ietf-hybi-websocket-perframe-compression
    """

    _DEFLATE_METHOD = 'deflate'

    def __init__(self, request):
        CompressionExtensionProcessorBase.__init__(self, request)

    def name(self):
        return common.PERFRAME_COMPRESSION_EXTENSION

    def _lookup_compression_processor(self, method_desc):
        if method_desc.name() == self._DEFLATE_METHOD:
            return DeflateFrameExtensionProcessor(method_desc)
        return None


_available_processors[common.PERFRAME_COMPRESSION_EXTENSION] = (
    PerFrameCompressionExtensionProcessor)


class DeflateMessageProcessor(ExtensionProcessorInterface):
    """Per-message deflate processor."""

    _S2C_MAX_WINDOW_BITS_PARAM = 's2c_max_window_bits'
    _S2C_NO_CONTEXT_TAKEOVER_PARAM = 's2c_no_context_takeover'
    _C2S_MAX_WINDOW_BITS_PARAM = 'c2s_max_window_bits'
    _C2S_NO_CONTEXT_TAKEOVER_PARAM = 'c2s_no_context_takeover'

    def __init__(self, request):
        self._request = request
        self._logger = util.get_class_logger(self)

        self._c2s_max_window_bits = None
        self._c2s_no_context_takeover = False
        self._bfinal = False

        self._compress_outgoing_enabled = False

        # True if a message is fragmented and compression is ongoing.
        self._compress_ongoing = False

        # Counters for statistics.

        # Total number of outgoing bytes supplied to this filter.
        self._total_outgoing_payload_bytes = 0
        # Total number of bytes sent to the network after applying this filter.
        self._total_filtered_outgoing_payload_bytes = 0

        # Total number of bytes received from the network.
        self._total_incoming_payload_bytes = 0
        # Total number of incoming bytes obtained after applying this filter.
        self._total_filtered_incoming_payload_bytes = 0

    def name(self):
        return 'deflate'

    def get_extension_response(self):
        # Any unknown parameter will be just ignored.

        s2c_max_window_bits = self._request.get_parameter_value(
            self._S2C_MAX_WINDOW_BITS_PARAM)
        if s2c_max_window_bits is not None:
            try:
                s2c_max_window_bits = int(s2c_max_window_bits)
            except ValueError, e:
                return None
            if s2c_max_window_bits < 8 or s2c_max_window_bits > 15:
                return None

        s2c_no_context_takeover = self._request.has_parameter(
            self._S2C_NO_CONTEXT_TAKEOVER_PARAM)
        if (s2c_no_context_takeover and
            self._request.get_parameter_value(
                self._S2C_NO_CONTEXT_TAKEOVER_PARAM) is not None):
            return None

        self._deflater = util._RFC1979Deflater(
            s2c_max_window_bits, s2c_no_context_takeover)

        self._inflater = util._RFC1979Inflater()

        self._compress_outgoing_enabled = True

        response = common.ExtensionParameter(self._request.name())

        if s2c_max_window_bits is not None:
            response.add_parameter(
                self._S2C_MAX_WINDOW_BITS_PARAM, str(s2c_max_window_bits))

        if s2c_no_context_takeover:
            response.add_parameter(
                self._S2C_NO_CONTEXT_TAKEOVER_PARAM, None)

        if self._c2s_max_window_bits is not None:
            response.add_parameter(
                self._C2S_MAX_WINDOW_BITS_PARAM,
                str(self._c2s_max_window_bits))
        if self._c2s_no_context_takeover:
            response.add_parameter(
                self._C2S_NO_CONTEXT_TAKEOVER_PARAM, None)

        self._logger.debug(
            'Enable %s extension ('
            'request: s2c_max_window_bits=%s; s2c_no_context_takeover=%r, '
            'response: c2s_max_window_bits=%s; c2s_no_context_takeover=%r)' %
            (self._request.name(),
             s2c_max_window_bits,
             s2c_no_context_takeover,
             self._c2s_max_window_bits,
             self._c2s_no_context_takeover))

        return response

    def setup_stream_options(self, stream_options):
        class _OutgoingMessageFilter(object):

            def __init__(self, parent):
                self._parent = parent

            def filter(self, message, end=True, binary=False):
                return self._parent._process_outgoing_message(
                    message, end, binary)

        class _IncomingMessageFilter(object):

            def __init__(self, parent):
                self._parent = parent
                self._decompress_next_message = False

            def decompress_next_message(self):
                self._decompress_next_message = True

            def filter(self, message):
                message = self._parent._process_incoming_message(
                    message, self._decompress_next_message)
                self._decompress_next_message = False
                return message

        self._outgoing_message_filter = _OutgoingMessageFilter(self)
        self._incoming_message_filter = _IncomingMessageFilter(self)
        stream_options.outgoing_message_filters.append(
            self._outgoing_message_filter)
        stream_options.incoming_message_filters.append(
            self._incoming_message_filter)

        class _OutgoingFrameFilter(object):

            def __init__(self, parent):
                self._parent = parent
                self._set_compression_bit = False

            def set_compression_bit(self):
                self._set_compression_bit = True

            def filter(self, frame):
                self._parent._process_outgoing_frame(
                    frame, self._set_compression_bit)
                self._set_compression_bit = False

        class _IncomingFrameFilter(object):

            def __init__(self, parent):
                self._parent = parent

            def filter(self, frame):
                self._parent._process_incoming_frame(frame)

        self._outgoing_frame_filter = _OutgoingFrameFilter(self)
        self._incoming_frame_filter = _IncomingFrameFilter(self)
        stream_options.outgoing_frame_filters.append(
            self._outgoing_frame_filter)
        stream_options.incoming_frame_filters.append(
            self._incoming_frame_filter)

        stream_options.encode_text_message_to_utf8 = False

    def set_c2s_max_window_bits(self, value):
        self._c2s_max_window_bits = value

    def set_c2s_no_context_takeover(self, value):
        self._c2s_no_context_takeover = value

    def set_bfinal(self, value):
        self._bfinal = value

    def enable_outgoing_compression(self):
        self._compress_outgoing_enabled = True

    def disable_outgoing_compression(self):
        self._compress_outgoing_enabled = False

    def _process_incoming_message(self, message, decompress):
        if not decompress:
            return message

        received_payload_size = len(message)
        self._total_incoming_payload_bytes += received_payload_size

        message = self._inflater.filter(message)

        filtered_payload_size = len(message)
        self._total_filtered_incoming_payload_bytes += filtered_payload_size

        _log_decompression_ratio(self._logger, received_payload_size,
                                 self._total_incoming_payload_bytes,
                                 filtered_payload_size,
                                 self._total_filtered_incoming_payload_bytes)

        return message

    def _process_outgoing_message(self, message, end, binary):
        if not binary:
            message = message.encode('utf-8')

        if not self._compress_outgoing_enabled:
            return message

        original_payload_size = len(message)
        self._total_outgoing_payload_bytes += original_payload_size

        message = self._deflater.filter(
            message, flush=end, bfinal=self._bfinal)

        filtered_payload_size = len(message)
        self._total_filtered_outgoing_payload_bytes += filtered_payload_size

        _log_compression_ratio(self._logger, original_payload_size,
                               self._total_outgoing_payload_bytes,
                               filtered_payload_size,
                               self._total_filtered_outgoing_payload_bytes)

        if not self._compress_ongoing:
            self._outgoing_frame_filter.set_compression_bit()
        self._compress_ongoing = not end
        return message

    def _process_incoming_frame(self, frame):
        if frame.rsv1 == 1 and not common.is_control_opcode(frame.opcode):
            self._incoming_message_filter.decompress_next_message()
            frame.rsv1 = 0

    def _process_outgoing_frame(self, frame, compression_bit):
        if (not compression_bit or
            common.is_control_opcode(frame.opcode)):
            return

        frame.rsv1 = 1


class PerMessageCompressionExtensionProcessor(
    CompressionExtensionProcessorBase):
    """WebSocket Per-message compression extension processor.

    Specification:
    http://tools.ietf.org/html/draft-ietf-hybi-permessage-compression
    """

    _DEFLATE_METHOD = 'deflate'

    def __init__(self, request):
        CompressionExtensionProcessorBase.__init__(self, request)

    def name(self):
        return common.PERMESSAGE_COMPRESSION_EXTENSION

    def _lookup_compression_processor(self, method_desc):
        if method_desc.name() == self._DEFLATE_METHOD:
            return DeflateMessageProcessor(method_desc)
        return None


_available_processors[common.PERMESSAGE_COMPRESSION_EXTENSION] = (
    PerMessageCompressionExtensionProcessor)


# Adding vendor-prefixed permessage-compress extension.
# TODO(bashi): Remove this after WebKit stops using vendor prefix.
_available_processors[common.X_WEBKIT_PERMESSAGE_COMPRESSION_EXTENSION] = (
    PerMessageCompressionExtensionProcessor)


class MuxExtensionProcessor(ExtensionProcessorInterface):
    """WebSocket multiplexing extension processor."""

    _QUOTA_PARAM = 'quota'

    def __init__(self, request):
        self._request = request

    def name(self):
        return common.MUX_EXTENSION

    def get_extension_response(self, ws_request,
                               logical_channel_extensions):
        # Mux extension cannot be used after extensions that depend on
        # frame boundary, extension data field, or any reserved bits
        # which are attributed to each frame.
        for extension in logical_channel_extensions:
            name = extension.name()
            if (name == common.PERFRAME_COMPRESSION_EXTENSION or
                name == common.DEFLATE_FRAME_EXTENSION or
                name == common.X_WEBKIT_DEFLATE_FRAME_EXTENSION):
                return None

        quota = self._request.get_parameter_value(self._QUOTA_PARAM)
        if quota is None:
            ws_request.mux_quota = 0
        else:
            try:
                quota = int(quota)
            except ValueError, e:
                return None
            if quota < 0 or quota >= 2 ** 32:
                return None
            ws_request.mux_quota = quota

        ws_request.mux = True
        ws_request.mux_extensions = logical_channel_extensions
        return common.ExtensionParameter(common.MUX_EXTENSION)

    def setup_stream_options(self, stream_options):
        pass


_available_processors[common.MUX_EXTENSION] = MuxExtensionProcessor


def get_extension_processor(extension_request):
    global _available_processors
    processor_class = _available_processors.get(extension_request.name())
    if processor_class is None:
        return None
    return processor_class(extension_request)


# vi:sts=4 sw=4 et
