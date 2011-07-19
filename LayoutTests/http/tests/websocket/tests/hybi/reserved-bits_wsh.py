import re
from mod_pywebsocket import common
from mod_pywebsocket import stream


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    match = re.search(r'\?bit=(\d+)$', request.ws_resource)
    if match is None:
        msgutil.send_message(request, 'FAIL: Query value is incorrect or missing')
        return

    bit = int(match.group(1))
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
