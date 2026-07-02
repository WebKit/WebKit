#!/usr/bin/env python3
import sys

sys.stdout.buffer.write(
    b'Content-Type: text/html\r\n'
    b'Access-control-allow-headers: X-Requested-With\r\n'
    b'Access-control-max-age: 0\r\n'
    b'Access-control-allow-origin: *\r\n'
    b'Access-control-allow-methods: *\r\n'
    b'Vary: Accept-Encoding\r\n'
    b'Content-Type: text/plain\r\n'
    b'\r\n'
    b'echo'
)

