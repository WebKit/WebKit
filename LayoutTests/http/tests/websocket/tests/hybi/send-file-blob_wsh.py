from mod_pywebsocket import common
from mod_pywebsocket import msgutil


def _retrieve_frame(stream):
    # FIXME: Use better API.
    frame = stream._receive_frame_as_frame_object()
    for frame_filter in stream._options.incoming_frame_filters:
        frame_filter.filter(frame)
    return frame


def web_socket_do_extra_handshake(request):
    pass # Always accept.


def web_socket_transfer_data(request):
    expected_messages = [b'Hello, world!']

    for test_number, expected_message in enumerate(expected_messages):
        frame = _retrieve_frame(request.ws_stream)
        if frame.opcode == common.OPCODE_BINARY and frame.payload == expected_message and frame.fin:
            msgutil.send_message(request, ('PASS: Message #%d.' % test_number).encode())
        else:
            msgutil.send_message(request, ('FAIL: Message #%d: Received unexpected frame: opcode = %r, payload = %r, final = %r' % (test_number, frame.opcode, frame.payload, frame.fin)).encode())


def all_distinct_bytes():
    return b''.join([i.to_bytes(1, 'big') for i in range(256)])
