#!/usr/bin/env python3

import json
import os
import sys

cookies = {}
if 'HTTP_COOKIE' in os.environ:
    header_cookies = os.environ['HTTP_COOKIE']
    header_cookies = header_cookies.split('; ')

    for cookie in header_cookies:
        cookie = cookie.split('=')
        cookies[cookie[0]] = cookie[1]

sys.stdout.write('Content-Type: text/html\r\n\r\n')

if len(cookies) == 0:
    print(json.dumps('No cookies'), end='')
else:
    print(json.dumps(cookies), end='')