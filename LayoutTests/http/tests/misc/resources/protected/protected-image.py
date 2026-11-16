#!/usr/bin/env python3

import base64
import os
import sys

uri = os.environ.get('REQUEST_URI', '')
username = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')[0]

sys.stdout.write(
    'Cache-Control: no-store\r\n'
    'Connection: close\r\n'
)

if not username:
    sys.stdout.write(
        'WWW-authenticate: Basic realm="{}"\r\n'
        'status: 401\r\n\r\n'.format(uri)
    )
else:
    with open(os.path.join(os.path.dirname(__file__), '../../../security/contentSecurityPolicy/block-all-mixed-content/resources/red-square.png'), 'rb') as file:
        sys.stdout.buffer.write(file.read())
        sys.stdout.write('Content-Type: image/png\r\n\r\n')
