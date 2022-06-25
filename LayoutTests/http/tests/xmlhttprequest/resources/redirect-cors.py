#!/usr/bin/env python3
import os
import sys
import time

from urllib.parse import parse_qs, unquote
query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)

sys.stdout.write('Content-Type: text/html\r\n')

url = query.get('url', [''])[0]

if os.environ.get('REQUEST_METHOD') == 'OPTIONS':
    if query.get('redirect-preflight', ['false'])[0] == 'true':
        sys.stdout.write(
            'status: 302\r\n'
            'Location: {}\r\n'.format(url)
        )
    else:
        sys.stdout.write('status: 200\r\n')
    sys.stdout.write(
        'Access-Control-Allow-Methods: GET\r\n'
        'Access-Control-Max-Age: 1\r\n'
    )

elif os.environ.get('REQUEST_METHOD') == 'GET':
    sys.stdout.write(
        'status: 302\r\n'
        'Location: {}\r\n'.format(url)
    )

allowOrigin = query.get('access-control-allow-origin', [None])[0]
allowCredentials = query.get('access-control-allow-credentials', [None])[0]
allowHeaders = query.get('access-control-allow-headers', [None])[0]

if allowOrigin is not None:
    sys.stdout.write('Access-Control-Allow-Origin: {}\r\n'.format(allowOrigin))
if allowCredentials is not None:
    sys.stdout.write('Access-Control-Allow-Credentials: {}\r\n'.format(allowCredentials))
if allowHeaders is not None:
    sys.stdout.write('Access-Control-Allow-Headers: {}\r\n'.format(allowHeaders))

sys.stdout.write('\r\n')
