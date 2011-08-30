def web_socket_do_extra_handshake(request):
    msg = "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
    msg += ("p" * 1024) + "\r\n"
    msg += "\r\n"
    msg += request.ws_challenge_md5
    request.connection.write(msg)
    raise Exception("Abort the connection") # Prevents pywebsocket from sending its own handshake message.


def web_socket_transfer_data(request):
    pass
