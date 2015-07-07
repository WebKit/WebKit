from mod_pywebsocket import common
from mod_pywebsocket import msgutil


def web_socket_do_extra_handshake(request):
    pass # Always accept.


def web_socket_transfer_data(request):
    expected_messages = ['Hello, world!', '', all_distinct_bytes()]

    for test_number, expected_message in enumerate(expected_messages):
        message = msgutil.receive_message(request)
        if type(message) == str and message == expected_message:
            msgutil.send_message(request, 'PASS: Message #%d.' % test_number)
        else:
            msgutil.send_message(request, 'FAIL: Message #%d: Received unexpected message: %r' % (test_number, message))


def all_distinct_bytes():
    return ''.join([chr(i) for i in xrange(256)])
