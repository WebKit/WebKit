#!/usr/bin/env python3

import base64
import os
import sys

credentials = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')
username = credentials[0]
password = ':'.join(credentials[1:])

uri = os.environ.get('REQUEST_URI', '')

sys.stdout.write(
    'Cache-Control: no-store\r\n'
    'Connection: close\r\n'
    'Content-Type: text/html\r\n'
)

if not username:
    sys.stdout.write(
        'WWW-authenticate: Basic realm="{}"\r\n'
        'status: 401\r\n\r\n'.format(uri)
    )
else:
    sys.stdout.write('\r\nAuthenticated as user: {} password: {}\n'.format(username, password))