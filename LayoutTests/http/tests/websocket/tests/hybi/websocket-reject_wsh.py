#!/usr/bin/env python3

from pywebsocket3 import handshake


def web_socket_do_extra_handshake(request):
    request.connection.write(b'HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n')
    raise handshake.AbortedByUserException('Reject the connection')


def web_socket_transfer_data(request):
    pass
