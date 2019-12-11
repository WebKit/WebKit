from mod_pywebsocket import msgutil


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    msgutil.send_message(request, request.headers_in['Sec-WebSocket-Key'])
