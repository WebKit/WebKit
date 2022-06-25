#!/usr/bin/env python3

import base64
import os
import sys

credentials = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')
username = credentials[0]
password = ':'.join(credentials[1:])

sys.stdout.write(
    'Access-Control-Allow-Origin: *\r\n'
    'Content-Type: text/html\r\n'
)

if not username or not password:
    sys.stdout.write(
        'WWW-Authenticate: Basic realm="WebKit Test Realm"\r\n'
        'status: 401\r\n\r\n'
        'Authentication canceled'
    )
    sys.exit(0)

sys.stdout.write('\r\nUser: {}, password: {}.'.format(username, password))
