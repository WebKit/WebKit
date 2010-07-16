from mod_pywebsocket import msgutil

def web_socket_do_extra_handshake(request):
    pass

def web_socket_transfer_data(request):
    msgutil.send_message(request, _hexify(request.ws_challenge))

def _hexify(bytes):
    return ':'.join(['%02X' % ord(byte) for byte in bytes])
