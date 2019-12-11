from mod_pywebsocket import common
from mod_pywebsocket import stream


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    payload = 'This text should be ignored. \xff' # '\xff' will never appear in UTF-8 encoded data.
    request.connection.write(stream.create_header(common.OPCODE_TEXT, len(payload), 1, 0, 0, 0, 0) + payload)
