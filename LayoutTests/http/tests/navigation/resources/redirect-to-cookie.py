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
    'status: 303\r\n'
    'Location: {}\r\n'
    'Content-Type: text/html\r\n\r\n'.format(cookies.get('location', ''))
)