#!/usr/bin/env python3

import os
import sys

cookies = {}
if 'HTTP_COOKIE' in os.environ:
    header_cookies = os.environ['HTTP_COOKIE']
    header_cookies = header_cookies.split('; ')

    for cookie in header_cookies:
        cookie = cookie.split('=')
        cookies[cookie[0]] = cookie[1]

sys.stdout.write(
    'Cache-Control: no-store\r\n'
    'Connection: close\r\n'
    'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
    'Access-Control-Allow-Credentials: true\r\n'
)

if not cookies.get('ModuleSecret', ''):
    sys.stdout.write(
        'Content-Type: text/html\r\n'
        'status: 401\r\n\r\n'
    )
    sys.exit(0)

sys.stdout.write('Content-Type: application/javascript\r\n\r\n\n')