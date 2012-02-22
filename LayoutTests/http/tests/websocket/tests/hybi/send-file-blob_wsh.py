from mod_pywebsocket import common
from mod_pywebsocket import msgutil


def web_socket_do_extra_handshake(request):
    pass # Always accept.


def web_socket_transfer_data(request):
    expected_messages = ['Hello, world!']

    for test_number, expected_message in enumerate(expected_messages):
        # FIXME: Use better API.
        opcode, payload, final, unused_reserved1, unused_reserved2, unused_reserved3 = request.ws_stream._receive_frame()
        if opcode == common.OPCODE_BINARY and payload == expected_message and final:
            msgutil.send_message(request, 'PASS: Message #%d.' % test_number)
        else:
            msgutil.send_message(request, 'FAIL: Message #%d: Received unexpected frame: opcode = %r, payload = %r, final = %r' % (test_number, opcode, payload, final))


def all_distinct_bytes():
    return ''.join([chr(i) for i in xrange(256)])
