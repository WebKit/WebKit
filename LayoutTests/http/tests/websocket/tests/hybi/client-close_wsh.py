from mod_pywebsocket import msgutil


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    # Wait for a close frame sent from the client.
    close_frame = request.ws_stream.receive_bytes(2)

    # Tell the client what we have received.
    msgutil.send_message(request, 'close_frame=%r' % close_frame)

    # If the following assertion fails, AssertionError will be raised,
    # which will prevent pywebsocket from sending a close frame.
    # In this case, the client will fail to finish closing handshake, thus
    # closeEvent.wasClean will become false.
    assert close_frame == '\xff\x00'

    # Pretend we have received a close frame from the client.
    # After this function exits, pywebsocket will send a close frame automatically.
    request.client_terminated = True
