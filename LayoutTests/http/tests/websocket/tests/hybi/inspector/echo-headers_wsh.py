from pywebsocket3 import msgutil


def web_socket_do_extra_handshake(request):
    pass  # Always accept.


def web_socket_transfer_data(request):
    # Echo the value of a header named by the client back to it.
    header_name = msgutil.receive_message(request)
    header_value = request.headers_in.get(header_name, "")
    msgutil.send_message(request, header_value)
