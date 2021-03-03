#!/usr/bin/env python3

import base64
import os
import sys

credentials = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')
username = credentials[0]
password = ':'.join(credentials[1:])

sys.stdout.write('Content-Type: text/html\r\n')

if not username or not password:
    sys.stdout.write(
        'WWW-Authenticate: Basic realm="WebKit Authentication Redirect 4"\r\n'
        'status: 401\r\n\r\n'
    )
else:
    sys.stdout.write(
        'Location: http://127.0.0.1:8000/misc/authentication-redirect-4/resources/auth-echo.py\r\n'
        'status: 301\r\n\r\n'
    )