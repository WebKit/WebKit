import struct
import time
from mod_pywebsocket import common


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    length = 0x8000000000000000

    # pywebsocket refuses to send a frame with too long payload.
    # Thus, we need to build a frame manually.
    header = (0x80 | common.OPCODE_TEXT).to_bytes(1, 'big')  # 0x80 is for "fin" bit.
    header += (127).to_bytes(1, 'big')
    header += struct.pack('!Q', length)
    request.connection.write(header)

    # Send data indefinitely to simulate a real (broken) server sending a big frame.
    # A client should ignore these bytes and abort the connection.
    while True:
        request.connection.write(b'X' * 4096)
        time.sleep(1)
