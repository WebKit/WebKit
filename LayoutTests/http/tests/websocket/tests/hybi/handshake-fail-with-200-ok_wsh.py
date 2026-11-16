from pywebsocket3 import handshake
from pywebsocket3.handshake.hybi import compute_accept_from_unicode


def web_socket_do_extra_handshake(request):
    # Reply with 200 OK instead of 101 Switching Protocols. The handshake
    # validation in the network stack fails on the wrong status code, so the
    # client surfaces an error and tears down the connection.
    msg = b"HTTP/1.1 200 OK\r\n"
    msg += b"Upgrade: websocket\r\n"
    msg += b"Connection: Upgrade\r\n"
    msg += b"Sec-WebSocket-Accept: "
    msg += compute_accept_from_unicode(request.headers_in["Sec-WebSocket-Key"])
    msg += b"\r\n"
    msg += b"\r\n"
    request.connection.write(msg)
    raise handshake.AbortedByUserException("Abort the connection")


def web_socket_transfer_data(request):
    pass
