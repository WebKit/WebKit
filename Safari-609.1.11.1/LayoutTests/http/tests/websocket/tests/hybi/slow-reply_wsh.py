from mod_pywebsocket import msgutil
import time

def web_socket_do_extra_handshake(request):
    pass

def web_socket_transfer_data(request):
    time.sleep(5)
    msgutil.send_message(request, 'Hello from Simple WSH.')