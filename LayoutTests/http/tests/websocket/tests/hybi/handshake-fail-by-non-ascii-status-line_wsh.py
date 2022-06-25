from mod_pywebsocket import handshake
from mod_pywebsocket import stream
from mod_pywebsocket.handshake.hybi import compute_accept_from_unicode

def web_socket_do_extra_handshake(request):
    frame = stream.create_text_frame('Frame-contains-thirty-one-bytes')

    message = frame
    message += 'HTTP/1.1 101 Switching Protocols non-ascií\r\n'.encode()
    message += b'Upgrade: websocket\r\n'
    message += b'Connection: Upgrade\r\n'
    message += b'Sec-WebSocket-Accept: '
    message += compute_accept_from_unicode(request.headers_in['Sec-WebSocket-Key'])
    message += b'\r\n\r\n'
    request.connection.write(message)
    raise handshake.AbortedByUserException('Abort the connection') # Prevents pywebsocket from sending its own handshake message.


def web_socket_transfer_data(request):
    pass
