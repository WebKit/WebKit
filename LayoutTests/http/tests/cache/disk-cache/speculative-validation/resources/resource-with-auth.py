#!/usr/bin/env python3

import base64
import os
import sys

credentials = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')
username = credentials[0]
password = ':'.join(credentials[1:])

expectedUsername = 'testUsername'
expectedPassword = 'testPassword'
realm = os.environ.get('REQUEST_URI', '')

sys.stdout.write(
    'Content-Type: text/javascript\r\n'
    'Cache-Control: max-age=0\r\n'
    'Etag: 123456789\r\n'
)

if username != expectedUsername or password != expectedPassword:
    sys.stdout.write(
        f'WWW-Authenticate: Basic realm="{realm}"\r\n'
        'status: 401\r\n\r\n'
        f'Sent username:password of ({username}:{password}) which is not what was expected\n'
    )
    sys.exit(0)

sys.stdout.write('\r\nloaded = true;\n')