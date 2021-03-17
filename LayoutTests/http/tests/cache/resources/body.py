#!/usr/bin/env python3

import sys

data = ''.join(sys.stdin.readlines())

sys.stdout.write(
    'Etag: 123456789\r\n'
    'Content-Type: text/html\r\n\r\n'
    f'{data}'
)