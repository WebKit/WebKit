#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

def send304():
    sys.stdout.write(
        'Content-Type: text/html\r\n'
        'status: 304\r\n'
        'ETag: foo\r\n\r\n'
    )

ident = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('id', [''])[0]
cookies = {}
if 'HTTP_COOKIE' in os.environ:
    header_cookies = os.environ['HTTP_COOKIE']
    header_cookies = header_cookies.split('; ')

    for cookie in header_cookies:
        cookie = cookie.split('=')
        cookies[cookie[0]] = cookie[1]

count = 1
if cookies.get(ident, None):
    count = int(cookies.get(ident, 0))

sys.stdout.write(f'Set-Cookie: {ident}={count + 1}\r\n')

if count == 1:
    sys.stdout.write(
        'Cache-Control: must-revalidate\r\n'
        'ETag: foo\r\n'
        'Content-Type: application/json\r\n\r\n'
        '{"version": 1}'
    )
elif count == 3:
    sys.stdout.write(
        'status: 404\r\n'
        'Cache-Control: must-revalidate\r\n'
        'Content-Type: application/json\r\n\r\n'
        '{"not": "found"}'
    )
elif count == 4:
    if os.environ.get('HTTP_IF_NONE_MATCH', '') == 'foo':
        send304()
    else:
        sys.stdout.write(
            'Cache-Control: must-revalidate\r\n'
            'ETag: foo\r\n'
            'Content-Type: application/json\r\n\r\n'
            '{"version": 2}'
        )
else:
    send304()