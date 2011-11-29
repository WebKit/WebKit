import time


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    # First frame (1 - 2 bytes): Zero-length binary frame with frame type 0x80.
    # Second frame (3 - 4 bytes): Close frame.
    data = '\x80\x00\xff\x00'

    # There is a bug that WebSocketChannel incorrectly consumes the first byte of a binary frame header
    # when it is split from the entire frame data. When this happens, the client recognizes the second and
    # third bytes ('\x00\xff') as a valid text frame and invokes a message event (which should not happen
    # in a valid client).

    request.connection.write(data[:1])
    time.sleep(0.5) # Wait for 500ms to make sure the first octet is sent out.
    request.connection.write(data[1:])

    raise handshake.AbortedByUserException('Abort the connection')
