#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
should_receive_cookies = query.get('shouldReceiveCookies', [None])[0]

cookies = {}
if 'HTTP_COOKIE' in os.environ:
    header_cookies = os.environ['HTTP_COOKIE']
    header_cookies = header_cookies.split('; ')

    for cookie in header_cookies:
        cookie = cookie.split('=')
        cookies[cookie[0]] = cookie[1]
        
first_party_cookie = cookies.get('firstPartyCookie', None)

sys.stdout.write('Content-Type: text/javascript\r\n\r\n')

if first_party_cookie:
    sys.stdout.write('let cookieResult = \'{}Did receive firstPartyCookie == {}\';'.format('PASS ' if should_receive_cookies is not None else 'FAIL ', first_party_cookie))
else:
    sys.stdout.write('let cookieResult = \'{}Did not receive cookie named firstPartyCookie\';'.format('FAIL ' if should_receive_cookies is not None else 'PASS '))

sys.stdout.write('postMessage(cookieResult);\n')
