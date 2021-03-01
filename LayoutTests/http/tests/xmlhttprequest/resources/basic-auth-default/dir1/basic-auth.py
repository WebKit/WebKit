#!/usr/bin/env python3
import base64
import os
import sys

sys.stdout.write('Content-Type: text/html\r\n')
if os.environ.get('HTTP_AUTHORIZATION'):
    credentials = base64.b64decode(os.environ['HTTP_AUTHORIZATION'].split(' ')[1]).decode().split(':')
    username = credentials[0]
    password = ':'.join(credentials[1:])
    sys.stdout.write(
        '\r\n'
        'User: {}, password: {}.'.format(username, password)
    )

else:
    sys.stdout.write(
        'WWW-Authenticate: Basic realm="xhr basic-auth-default"\r\n'
        'status: 401\r\n'
        '\r\n'
        'Authentication canceled'
    )
