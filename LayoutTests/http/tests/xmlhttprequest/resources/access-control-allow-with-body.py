#!/usr/bin/env python3
import sys

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Access-control-allow-headers: X-Requested-With\r\n'
    'Access-control-max-age: 0\r\n'
    'Access-control-allow-origin: *\r\n'
    'Access-control-allow-methods: *\r\n'
    'Vary: Accept-Encoding\r\n'
    'Content-Type: text/plain\r\n'
    '\r\n'
    'echo'
)

