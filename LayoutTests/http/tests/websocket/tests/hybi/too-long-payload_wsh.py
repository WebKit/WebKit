import struct
import time
from mod_pywebsocket import common


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    length = 0x8000000000000000

    # pywebsocket refuses to send a frame with too long payload.
    # Thus, we need to build a frame manually.
    header = chr(0x80 | common.OPCODE_TEXT) # 0x80 is for "fin" bit.
    header += chr(127)
    header += struct.pack('!Q', length)
    request.connection.write(header)

    # Send data indefinitely to simulate a real (broken) server sending a big frame.
    # A client should ignore these bytes and abort the connection.
    while True:
        request.connection.write('X' * 4096)
        time.sleep(1)
