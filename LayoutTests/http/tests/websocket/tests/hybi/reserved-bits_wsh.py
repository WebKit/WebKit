import re
from mod_pywebsocket import common
from mod_pywebsocket import stream
from mod_pywebsocket.extensions import DeflateFrameExtensionProcessor

bit = 0

def _get_deflate_frame_extension_processor(request):
    for extension_processor in request.ws_extension_processors:
        if isinstance(extension_processor, DeflateFrameExtensionProcessor):
            return extension_processor
    return None


def web_socket_do_extra_handshake(request):
    match = re.search(r'\?compressed=(true|false)&bitNumber=(\d)$', request.ws_resource)
    if match is None:
        msgutil.send_message(request, 'FAIL: Query value is incorrect or missing')
        return
    
    global bit
    compressed = match.group(1)
    bit = int(match.group(2))
    if compressed == "false":
        request.ws_extension_processors = [] # using no extension response
    else:
        processor = _get_deflate_frame_extension_processor(request)
        if not processor:
            request.ws_extension_processors = [] # using no extension response


def web_socket_transfer_data(request):
    message = "This message should be ignored."
    if bit == 1:
        frame = stream.create_header(common.OPCODE_TEXT, len(message), 1, 1, 0, 0, 0) + message
    elif bit == 2:
        frame = stream.create_header(common.OPCODE_TEXT, len(message), 1, 0, 1, 0, 0) + message
    elif bit == 3:
        frame = stream.create_header(common.OPCODE_TEXT, len(message), 1, 0, 0, 1, 0) + message
    else:
        frame = stream.create_text_frame('FAIL: Invalid bit number: %d' % bit)
    request.connection.write(frame)
