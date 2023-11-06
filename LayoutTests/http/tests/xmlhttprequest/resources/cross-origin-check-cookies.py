#!/usr/bin/env python3
import os
import sys

from urllib.parse import parse_qs
query = parse_qs(os.environ.get('QUERY_STRING', ''))
cookies = {
    pair.split('=')[0]: '='.join(pair.split('=')[1:]) for pair in os.environ.get('HTTP_COOKIE', '').split(';')
}

sys.stdout.buffer.write(
    b'Content-Type: text/html\r\n'
    b'Cache-Control: no-store\r\n'
    b'Last-Modified: Thu, 19 Mar 2009 11:22:11 GMT\r\n'
    b'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
    b'Access-Control-Allow-Credentials: true\r\n'
    b'Connection: close\r\n'
    b'\r\n'
)

cookie = query.get('cookie')
if cookie:
    cookie = cookie[0]
if not cookie:
    cookie = 'WK-cross-origin'

if cookies.get(cookie):
    sys.stdout.buffer.write('{}: {}'.format(cookie, cookies.get(cookie)).encode())
else:
    sys.stdout.buffer.write(b'Cookie was not sent')
