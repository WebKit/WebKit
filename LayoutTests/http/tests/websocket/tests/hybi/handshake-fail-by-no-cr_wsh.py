from mod_pywebsocket import handshake
from mod_pywebsocket.handshake.hybi import compute_accept_from_unicode


def web_socket_do_extra_handshake(request):
    msg = b'HTTP/1.1 101 Switching Protocols\n'  # Does not end with "\r\n".
    msg += b'Upgrade: websocket\r\n'
    msg += b'Connection: Upgrade\r\n'
    msg += b'Sec-WebSocket-Accept: '
    msg += compute_accept_from_unicode(request.headers_in['Sec-WebSocket-Key'])
    msg += b'\r\n\r\n'
    request.connection.write(msg)
    print(msg)
    raise handshake.AbortedByUserException('Abort the connection')  # Prevents pywebsocket from sending its own handshake message.


def web_socket_transfer_data(request):
    pass
