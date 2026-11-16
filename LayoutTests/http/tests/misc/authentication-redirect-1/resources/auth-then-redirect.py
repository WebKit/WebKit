#!/usr/bin/env python3

import base64
import os
import sys
from urllib.parse import parse_qs

credentials = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')
username = credentials[0]
password = ':'.join(credentials[1:])

redirect = query = int(parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('redirect', [200])[0])

sys.stdout.write('Content-Type: text/html\r\n')

if not username or not password:
    sys.stdout.write(
        'WWW-Authenticate: Basic realm="http/tests/misc/authentication-redirect-1"\r\n'
        'status: 401\r\n\r\n'
    )
else:
    if redirect in [301, 302, 303, 307]:
        sys.stdout.write(
            'Location: http://127.0.0.1:8000/misc/authentication-redirect-1/resources/auth-echo.py\r\n'
            'status: {}\r\n\r\n'.format(redirect)
        )
    else:
        sys.stdout.write('\r\nUnknown redirect parameter sent')