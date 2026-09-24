def web_socket_do_extra_handshake(request):
    request.extra_headers.extend([
        ('X-Example', 'one,two'),
        ('X-Example', 'three'),
        ('WWW-Authenticate', 'Digest realm="one,two", nonce="three,four"'),
        ('WWW-Authenticate', 'Basic realm="five,six"'),
        ('Set-Cookie', 'handshake=value; Expires=Wed, 21 Oct 2037 07:28:00 GMT'),
    ])


def web_socket_transfer_data(request):
    request.ws_stream.close_connection()
