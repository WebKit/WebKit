from mod_pywebsocket import msgutil


def web_socket_do_extra_handshake(request):
    pass # Always accept.


def web_socket_transfer_data(request):
    # Echo message back
    message = msgutil.receive_message(request)
    msgutil.send_message(request, message)
