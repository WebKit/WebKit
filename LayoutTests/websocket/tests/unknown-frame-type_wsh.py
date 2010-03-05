from mod_pywebsocket import msgutil

def web_socket_do_extra_handshake(request):
    pass

def web_socket_transfer_data(request):
    for i in range(1, 128):
        request.connection.write(chr(i) + str(i) + '\xff')
    for i in range(128, 256):
        msg = str(i)
        request.connection.write(chr(i) + chr(len(msg)) + msg)
    msgutil.send_message(request, 'done')
