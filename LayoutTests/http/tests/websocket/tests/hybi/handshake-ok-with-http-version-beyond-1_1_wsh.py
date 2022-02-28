from mod_pywebsocket import handshake
from mod_pywebsocket.handshake.hybi import compute_accept_from_unicode


def web_socket_do_extra_handshake(request):

    resources = request.ws_resource.split('?', 1)
    parameter = None
    message = b''
    if len(resources) == 2:
        parameter = resources[1]
        message += b'HTTP/'
        message += parameter.encode()
        message += b' 101 Switching Protocols\r\n'
    message += b'Upgrade: websocket\r\n'
    message += b'Connection: Upgrade\r\n'
    message += b'Sec-WebSocket-Accept: '
    message += compute_accept_from_unicode(request.headers_in['Sec-WebSocket-Key'])
    message += b'\r\n\r\n'
    request.connection.write(message)
    raise handshake.AbortedByUserException('Abort the connection') # Prevents pywebsocket from sending its own handshake message.


def web_socket_transfer_data(request):
    pass
