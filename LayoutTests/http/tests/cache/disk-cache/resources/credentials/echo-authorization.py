#!/usr/bin/env python3
import os
import sys
from urllib.parse import parse_qs

query_string = os.environ.get('QUERY_STRING', '')
query = parse_qs(query_string, keep_blank_values=True)
authorization = os.environ.get('HTTP_AUTHORIZATION')

if 'challenge' in query and not authorization:
    sys.stdout.write(
        'status: 401\r\n'
        'WWW-Authenticate: Basic realm="WebKit Test Realm"\r\n'
        '\r\n'
    )
    sys.exit(0)

if 'redirect' in query:
    sys.stdout.write(
        'status: 302\r\n'
        'Location: echo-authorization.py?{}\r\n'
        '\r\n'.format(query_string.replace('redirect&', '', 1))
    )
    sys.exit(0)

body = authorization or 'none'
content_type = 'text/plain'
if 'script' in query:
    body = 'scriptResult = "{}";'.format(body)
    content_type = 'text/javascript'

sys.stdout.write(
    'Content-Type: {}\r\n'
    'Cache-Control: max-age=100\r\n'
    'Access-Control-Allow-Origin: *\r\n'
    '\r\n'
    '{}'.format(content_type, body)
)
