#!/usr/bin/env python3

import os
import sys

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    'Cookies are:\n'
)

if 'HTTP_COOKIE' in os.environ:
    header_cookies = os.environ['HTTP_COOKIE']
    header_cookies = header_cookies.split('; ')

    for cookie in header_cookies:
        cookie = cookie.split('=')
        sys.stdout.write('{} = {}\n'.format(cookie[0], cookie[1]))