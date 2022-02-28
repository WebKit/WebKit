from mod_pywebsocket import handshake
from mod_pywebsocket.handshake.hybi import compute_accept_from_unicode


def web_socket_do_extra_handshake(request):
    message = b'HTTP/1.1 101 Switching Protocols\r\n'
    message += b'Upgrade: websocket\r\n'
    message += b'Connection: Upgrade\r\n'
    message += b'Sec-WebSocket-Origin: http://localhost:8880\r\n'
    message += b'Sec-WebSocket-Location: ws://localhost:8880/bogus\r\n'
    message += b'Sec-WebSocket-Accept: '
    message += compute_accept_from_unicode(request.headers_in['Sec-WebSocket-Key'])
    message += 'b\r\n\r\n'
    request.connection.write(message)
    raise handshake.AbortedByUserException('Abort the connection') # Prevents pywebsocket from sending its own handshake message.


def web_socket_transfer_data(request):
    pass
