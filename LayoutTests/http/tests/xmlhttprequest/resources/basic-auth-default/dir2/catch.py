#!/usr/bin/env python3
import base64
import os
import sys

sys.stdout.buffer.write(b'Content-Type: text/html\r\n')

credentials = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')
username = credentials[0]
password = ':'.join(credentials[1:]) if len(credentials) > 1 else ''

if username:
    sys.stdout.buffer.write(
        '\r\n'
        'User: {}, password: {}.'.format(username, password).encode()
    )

else:
    sys.stdout.buffer.write(
        b'status: 500\r\n'
        b'\r\n'
        b'Where is my default auth?'
    )
