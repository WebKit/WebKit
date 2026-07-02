#!/usr/bin/env python3
import base64
import os
import sys

sys.stdout.buffer.write(
    b'Content-Type: text/html\r\n'
    b'Cache-Control: no-cache, no-store\r\n'
)

credentials = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')
username = credentials[0]
password = ':'.join(credentials[1:])

if username and password:
    sys.stdout.buffer.write(
        '\r\n'
        'User: {}, password: {}.'.format(username, password).encode()
    )

else:
    sys.stdout.buffer.write(
        b'WWW-Authenticate: Basic realm="WebKit test re-login"\r\n'
        b'status: 401\r\n'
        b'\r\n'
        b'Authentication canceled'
    )
