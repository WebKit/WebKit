from mod_pywebsocket import common
from mod_pywebsocket import msgutil
from mod_pywebsocket import stream


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    # Fragmented control frame is prohibited. The client must abort the connection.
    request.connection.write(stream.create_text_frame('This message should be ignored.', opcode=common.OPCODE_PING, fin=0))
