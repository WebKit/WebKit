#!/usr/bin/env python3
import base64
import os
import sys

from urllib.parse import parse_qs
query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)

cookies = {
    pair.split('=')[0]: '='.join(pair.split('=')[1:]) for pair in os.environ.get('HTTP_COOKIE', '').split(';')
}

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
    'Access-Control-Allow-Credentials: true\r\n'
)

if os.environ.get('REQUEST_METHOD') == 'OPTIONS':
    if cookies.get('foo') is None:
        sys.stdout.write('Access-Control-Allow-Methods: PUT\r\n')
    sys.stdout.write('\r\n')

else:
    credentials = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')
    username = credentials[0]
    password = ':'.join(credentials[1:])

    if username == query.get('uid', [''])[0]:
        sys.stdout.write(
            '\r\n'
            'User: {}, password: {}.'.format(username, password)
        )

    else:
        sys.stdout.write(
            'WWW-Authenticate: Basic realm="WebKit Test Realm/Cross Origin"\r\n'
            'status: 401\r\n'
            '\r\n'
            'Authentication canceled'
        )
