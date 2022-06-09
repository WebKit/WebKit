import struct

from mod_pywebsocket import msgutil
from mod_pywebsocket import stream

def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    # Wait for a close frame sent from the client.
    close_frame = request.ws_stream.receive_bytes(6)

    msgutil.send_message(request, 'Client should ignore this message')

    # Send only first two bytes of the received frame. The remaining four bytes are
    # "masking key", which changes every time the test runs.
    data = struct.pack('!H', 1000) + 'close_frame[:2]=%r' % close_frame[:2]
    request.connection.write(stream.create_close_frame(data))

    # If the following assertion fails, AssertionError will be raised,
    # which will prevent pywebsocket from sending a close frame.
    # In this case, the client will fail to finish closing handshake, thus
    # closeEvent.wasClean will become false.
    assert close_frame[:2] == '\x88\x82'

    # Pretend we have received a close frame from the client.
    # After this function exits, pywebsocket will send a close frame automatically.
    request.client_terminated = True
