#!/usr/bin/env python3

import base64
import os
import sys
from urllib.parse import parse_qs

credentials = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')
username = credentials[0]
password = ':'.join(credentials[1:])

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)

expectedUsername = query.get('username', ['username'])[0]
expectedPassword = query.get('password', ['password'])[0]
realm = query.get('realm', [os.environ.get('REQUEST_URI', '')])[0]

sys.stdout.write(
    'Cache-Control: no-store\r\n'
    'Content-Type: text/html\r\n'
)

if username != expectedUsername or password != expectedPassword:
    sys.stdout.write(
        'WWW-Authenticate: Basic realm="{}"\r\n'
        'status: 401\r\n\r\n'
        'Sent username:password of ({}:{}) which is not what was expected\n'.format(realm, username, password)
    )
else:
    sys.stdout.write('\r\nAuthenticated as user: {} password: {}'.format(username, password))