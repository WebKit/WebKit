#!/usr/bin/env python3

import sys

s = '12345678'
for i in range(0, 6):
    s += s

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Content-Length: {}\r\n'
    'status: 404\r\n'
    '\r\n'.format(len(s))
)

sys.stdout.write(s)