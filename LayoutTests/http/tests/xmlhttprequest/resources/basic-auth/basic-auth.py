#!/usr/bin/env python3
import base64
import os
import sys

from urllib.parse import parse_qs
query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)

sys.stdout.write('Content-Type: text/html\r\n')
if os.environ.get('HTTP_AUTHORIZATION'):
    credentials = base64.b64decode(os.environ['HTTP_AUTHORIZATION'].split(' ')[1]).decode().split(':')
    username = credentials[0]
    password = ':'.join(credentials[1:])
    if query.get('uid', [''])[0] == username:
        sys.stdout.write(
            '\r\n'
            'User: {}, password: {}.'.format(username, password)
        )
        sys.exit(0)

sys.stdout.write(
    'WWW-Authenticate: Basic realm="WebKit Test Realm"\r\n'
    'status: 401\r\n'
    '\r\n'
    'Authentication canceled'
)
