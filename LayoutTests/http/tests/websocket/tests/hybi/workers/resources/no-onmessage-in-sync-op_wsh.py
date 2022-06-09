from mod_pywebsocket import handshake
from mod_pywebsocket import msgutil


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    # Send three messages, and then wait for three messages.
    msgutil.send_message(request, '1')
    msgutil.send_message(request, '2')
    msgutil.send_message(request, '3')

    for expected in (u'1', u'2', u'3'):
        message = msgutil.receive_message(request)
        if type(message) != unicode or message != expected:
            raise handshake.AbortedByUserException('Abort the connection')
