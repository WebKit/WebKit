#!/usr/bin/env python3
import base64
import os
import sys

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Cache-Control: no-cache, no-store\r\n'
)

credentials = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')
username = credentials[0]
password = ':'.join(credentials[1:])

if (username, password) == ('user', 'pass'):
    sys.stdout.write(
        '\r\n'
        'User: {}, password: {}.'.format(username, password)
    )

else:
    sys.stdout.write(
        'WWW-Authenticate: Basic realm="Please cancel this authentication dialog"\r\n'
        'status: 401\r\n'
        '\r\n'
        'Authentication canceled'
    )
