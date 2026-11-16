#!/usr/bin/env python3
import base64
import os
import sys

sys.stdout.buffer.write(b'Content-Type: text/html\r\n')
if os.environ.get('HTTP_AUTHORIZATION'):
    credentials = base64.b64decode(os.environ['HTTP_AUTHORIZATION'].split(' ')[1]).decode().split(':')
    username = credentials[0]
    password = ':'.join(credentials[1:])
    sys.stdout.buffer.write(
        '\r\n'
        'User: {}, password: {}.'.format(username, password).encode()
    )

else:
    sys.stdout.buffer.write(
        b'WWW-Authenticate: Basic realm="xhr basic-auth-default"\r\n'
        b'status: 401\r\n'
        b'\r\n'
        b'Authentication canceled'
    )
