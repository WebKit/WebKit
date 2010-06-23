def web_socket_do_extra_handshake(request):
    msg = 'HTTP/1.1 101 WebSocket Protocol Handshake\n' # Does not end with "\r\n".
    msg += 'Upgrade: WebSocket\r\n'
    msg += 'Connection: Upgrade\r\n'
    msg += 'Sec-WebSocket-Location: ' + request.ws_location + '\r\n'
    msg += 'Sec-WebSocket-Origin: ' + request.ws_origin + '\r\n'
    msg += '\r\n'
    msg += request.ws_challenge_md5
    request.connection.write(msg)
    print msg

def web_socket_transfer_data(request):
    pass
