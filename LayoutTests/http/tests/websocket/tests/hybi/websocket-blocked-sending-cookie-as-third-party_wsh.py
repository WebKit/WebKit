#!/usr/bin/env python3

from mod_pywebsocket import handshake
from mod_pywebsocket.handshake.hybi import compute_accept_from_unicode


class HeaderCache:
    previousHeaders = ""


def web_socket_do_extra_handshake(request):
    message = b''

    if request.headers_in['Cookie']:
        HeaderCache.previousHeaders = request.headers_in['Cookie']
        message += b'HTTP/1.1 403\r\n\r\n'
        request.connection.write(message)
        raise handshake.AbortedByUserException('Abort the connection')  # Prevents pywebsocket from sending its own handshake message.


def web_socket_transfer_data(request):
    request.ws_stream.send_message('Cookies are: {}\r\n'.format(HeaderCache.previousHeaders), binary=False)
