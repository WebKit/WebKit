#!/usr/bin/env python3

import base64
import os
import sys

username = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')[0]

uri = os.environ.get('REQUEST_URI', '')

sys.stdout.write(
    'Cache-Control: no-store\r\n'
    'Connection: close\r\n'
)

if not username:
    sys.stdout.write(
        'WWW-authenticate: Basic realm="{}"\r\n'
        'Content-Type: text/html\r\n'
        'status: 401\r\n\r\n'.format(uri)
    )
    sys.exit(0)

sys.stdout.write('Content-Type: image/png\r\n\r\n')
sys.stdout.flush()
with open(os.path.join(os.path.dirname(__file__), '../../contentSecurityPolicy/block-all-mixed-content/resources/red-square.png'), 'rb') as file:
    sys.stdout.buffer.write(file.read())
