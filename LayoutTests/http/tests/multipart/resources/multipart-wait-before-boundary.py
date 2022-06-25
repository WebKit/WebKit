#!/usr/bin/env python3

import os
import sys
import time
from urllib.parse import parse_qs

done = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('done', [None])[0]

if done is not None:
    sys.stdout.write(
        'Content-Type: text/html\r\n\r\n'
        '<script>parent.success()</script>'
    )
    sys.exit(0)

boundary = 'cutHere'
padding = 'a' * 2048
def sendHeader():
    sys.stdout.write(
        '--{}\r\n'
        'Content-Type: text/html\r\n\r\n'.format(boundary)
    )
    sys.stdout.flush()

sys.stdout.write('Content-Type: multipart/x-mixed-replace; boundary={}\r\n\r\n'.format(boundary))

sendHeader()
sys.stdout.write(
    'test html\n'
    '<!-- {} -->'.format(padding)
)
sys.stdout.flush()

sendHeader()
sys.stdout.write(
    'second html\n'
    '<script>parent.childLoaded()</script>'
    '<!-- {} -->'.format(padding)
)
sys.stdout.flush()

sendHeader()
sys.stdout.write(
    'third html\n'
    '<!-- {} -->'.format(padding)
)
sys.stdout.flush()

time.sleep(20)