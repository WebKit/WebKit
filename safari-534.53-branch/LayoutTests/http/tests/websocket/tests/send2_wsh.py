def web_socket_do_extra_handshake(request):
    pass # Always accept.

def web_socket_transfer_data(request):
    # send 2 messages in one packet.
    request.connection.write("\x00" + "first message" + "\xff" +
                             "\x00" + "second message" + "\xff")
