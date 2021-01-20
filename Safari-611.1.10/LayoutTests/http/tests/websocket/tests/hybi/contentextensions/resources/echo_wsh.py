from mod_pywebsocket import msgutil

def web_socket_do_extra_handshake(request):
    pass

def web_socket_transfer_data(request):
    for unused in range(1):
        message = msgutil.receive_message(request)
        msgutil.send_message(request, "reply " + message)
