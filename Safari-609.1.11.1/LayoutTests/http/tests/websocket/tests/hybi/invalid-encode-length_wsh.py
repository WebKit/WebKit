import re
import struct
from mod_pywebsocket import common


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    match = re.search(r'\?case=(\d+_\d+)$', request.ws_resource)
    if match is None:
        msgutil.send_message(request, 'FAIL: Query value is incorrect or missing')
        return

    payload_length, extended_length = (match.group(1)).split('_', 1)
    payload_length = int(payload_length)
    extended_length = int(extended_length)

    # pywebsocket refuses to create a frame with error encode length.
    # Thus, we need to build a frame manually.
    header = chr(0x80 | common.OPCODE_TEXT) # 0x80 is for "fin" bit.
    header += chr(payload_length) # No Mask and two bytes extended payload length.
    if payload_length == 126:
        header += struct.pack('!H', extended_length)
    elif payload_length == 127:
        header += struct.pack('!Q', extended_length)
    else:
        msgutil.send_message(request, 'FAIL: Query value is incorrect or missing')
        return
    request.connection.write(header)
    request.connection.write('X' * extended_length)
