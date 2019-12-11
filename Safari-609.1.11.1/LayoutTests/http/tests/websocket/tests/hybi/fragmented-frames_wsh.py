from mod_pywebsocket import msgutil


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    # send_message's third argument corresponds to "fin" bit;
    # it is set to True if this frame is the final fragment of a message.
    msgutil.send_message(request, 'First ', False)
    msgutil.send_message(request, 'message', True)

    # Empty fragment is allowed.
    msgutil.send_message(request, '', False)
    msgutil.send_message(request, 'Second ', False)
    msgutil.send_message(request, '', False)
    msgutil.send_message(request, 'message', False)
    msgutil.send_message(request, '', True)

    # Fragment aggressively.
    msgutil.send_message(request, 'T', False)
    msgutil.send_message(request, 'h', False)
    msgutil.send_message(request, 'i', False)
    msgutil.send_message(request, 'r', False)
    msgutil.send_message(request, 'd', False)
    msgutil.send_message(request, ' ', False)
    msgutil.send_message(request, 'm', False)
    msgutil.send_message(request, 'e', False)
    msgutil.send_message(request, 's', False)
    msgutil.send_message(request, 's', False)
    msgutil.send_message(request, 'a', False)
    msgutil.send_message(request, 'g', False)
    msgutil.send_message(request, 'e', True)
