def web_socket_do_extra_handshake(request):
    request.ws_protocol = request.ws_requested_protocols[0]


def web_socket_transfer_data(request):
    pass
