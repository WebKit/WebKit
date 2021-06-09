#!/usr/bin/env python3

import base64
import os
import sys

credentials = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')
username = credentials[0]
password = ':'.join(credentials[1:])

if username == 'testusername' and password == 'testpassword':
    sys.stdout.write('Content-Type: image/png\r\n\r\n')
    sys.stdout.flush()
    with open(os.path.join(os.path.dirname(__file__), 'black-square.png'), 'rb') as file:
        sys.stdout.buffer.write(file.read())
    sys.exit(0)

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'status: 401\r\n'
    'WWW-Authenticate: Basic realm="test realm"\r\n\r\n'
)
