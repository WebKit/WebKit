from mod_pywebsocket import handshake
from mod_pywebsocket import stream
from mod_pywebsocket.handshake.hybi import compute_accept

def web_socket_do_extra_handshake(request):
    frame = stream.create_text_frame('Frame-contains-thirty-one-bytes')

    message = frame
    message += 'HTTP/1.1 101 Switching Protocols non-ascií\r\n'
    message += 'Upgrade: websocket\r\n'
    message += 'Connection: Upgrade\r\n'
    message += 'Sec-WebSocket-Accept: %s\r\n' % compute_accept(request.headers_in['Sec-WebSocket-Key'])[0]
    message += '\r\n'
    request.connection.write(message)
    raise handshake.AbortedByUserException('Abort the connection') # Prevents pywebsocket from sending its own handshake message.


def web_socket_transfer_data(request):
    pass
