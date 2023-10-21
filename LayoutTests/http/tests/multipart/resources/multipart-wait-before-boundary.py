#!/usr/bin/env python3

import os
import sys
import time
from urllib.parse import parse_qs

done = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('done', [None])[0]

if done is not None:
    sys.stdout.buffer.write(
        b'Content-Type: text/html\r\n\r\n'
        b'<script>parent.success()</script>'
    )
    sys.exit(0)

boundary = 'cutHere'
padding = 'a' * 2048
def sendHeader():
    sys.stdout.buffer.write(
        '--{}\r\n'
        'Content-Type: text/html\r\n\r\n'.format(boundary).encode()
    )
    sys.stdout.flush()


sys.stdout.buffer.write('Content-Type: multipart/x-mixed-replace; boundary={}\r\n\r\n'.format(boundary).encode())

sendHeader()
sys.stdout.buffer.write(
    'test html\n'
    '<!-- {} -->'.format(padding).encode()
)
sys.stdout.flush()

sendHeader()
sys.stdout.buffer.write(
    'second html\n'
    '<script>parent.childLoaded()</script>'
    '<!-- {} -->'.format(padding).encode()
)
sys.stdout.flush()

sendHeader()
sys.stdout.buffer.write(
    'third html\n'
    '<!-- {} -->'.format(padding).encode()
)
sys.stdout.flush()

time.sleep(20)
