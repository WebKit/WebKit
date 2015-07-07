# Copyright 2012, Google Inc.  All rights reserved.
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


import urlparse
from mod_pywebsocket.extensions import DeflateFrameExtensionProcessor
from mod_pywebsocket.extensions import ExtensionProcessorInterface
from mod_pywebsocket.common import ExtensionParameter


_GOODBYE_MESSAGE = u'Goodbye'
_ENABLE_MESSAGE = u'EnableCompression'
_DISABLE_MESSAGE = u'DisableCompression'


def _get_deflate_frame_extension_processor(request):
    for extension_processor in request.ws_extension_processors:
        if isinstance(extension_processor, DeflateFrameExtensionProcessor):
            return extension_processor
    return None


def web_socket_do_extra_handshake(request):
    processor = _get_deflate_frame_extension_processor(request)
    if not processor:
        return
    r = request.ws_resource.split('?', 1)
    if len(r) == 1:
        return
    parameters = urlparse.parse_qs(r[1], keep_blank_values=True)
    if 'max_window_bits' in parameters:
        window_bits = int(parameters['max_window_bits'][0])
        processor.set_response_window_bits(window_bits)
    if 'no_context_takeover' in parameters:
        processor.set_response_no_context_takeover(True)
    if 'set_bfinal' in parameters:
        processor.set_bfinal(True)


def web_socket_transfer_data(request):
    processor = _get_deflate_frame_extension_processor(request)
    while True:
        line = request.ws_stream.receive_message()
        if line is None:
            return
        if isinstance(line, unicode):
            if processor:
                if line == _ENABLE_MESSAGE:
                    processor.enable_outgoing_compression()
                elif line == _DISABLE_MESSAGE:
                    processor.disable_outgoing_compression()
            request.ws_stream.send_message(line, binary=False)
            if line == _GOODBYE_MESSAGE:
                return
        else:
            request.ws_stream.send_message(line, binary=True)


# vi:sts=4 sw=4 et
