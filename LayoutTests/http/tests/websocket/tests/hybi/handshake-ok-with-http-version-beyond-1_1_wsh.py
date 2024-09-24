from mod_pywebsocket import handshake, stream
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
    # <https://github.com/web-platform-tests/wpt/blob/e3af2c724e7ddd08425d3fc0a26fda1637242436/websockets/handlers/simple_handshake_wsh.py#L17-L30>
    # Create a clean close frame.
    close_body = stream.create_closing_handshake_body(1001, 'PASS')
    close_frame = stream.create_close_frame(close_body)
    request.connection.write(close_frame)
    # Wait for the responding close frame from the user agent.
    MASK_LENGTH = 4
    request.connection.read(len(close_frame) + MASK_LENGTH)
    raise handshake.AbortedByUserException('Abort the connection') # Prevents pywebsocket from sending its own handshake message.


def web_socket_transfer_data(request):
    pass
