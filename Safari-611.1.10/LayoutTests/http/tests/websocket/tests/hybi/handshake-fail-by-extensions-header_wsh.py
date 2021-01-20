from mod_pywebsocket import handshake
from mod_pywebsocket.handshake.hybi import compute_accept


def web_socket_do_extra_handshake(request):
    message = 'HTTP/1.1 101 Switching Protocols\r\n'
    message += 'Upgrade: websocket\r\n'
    message += 'Connection: Upgrade\r\n'
    message += 'Sec-WebSocket-Accept: %s\r\n' % compute_accept(request.headers_in['Sec-WebSocket-Key'])[0]
    message += 'Sec-WebSocket-Extensions: x-foo\r\n'
    message += '\r\n'
    request.connection.write(message)
    raise handshake.AbortedByUserException('Abort the connection') # Prevents pywebsocket from sending its own handshake message.


def web_socket_transfer_data(request):
    pass
