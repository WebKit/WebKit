#!/usr/bin/env python3

import os
import sys
from datetime import datetime, timedelta
from urllib.parse import parse_qs

step = int(parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('step', [0])[0])

sys.stdout.write('Content-Type: text/html\r\n')

if step == 1:
    sys.stdout.write(
        'status: 302\r\n'
        'Location: http://localhost:8000/cookies/resources/set-cookie-on-redirect.py?step=2\r\n\r\n'
    )
elif step == 2:
    expires = datetime.utcnow() + timedelta(seconds=86400)
    sys.stdout.write(
        'status: 302\r\n'
        'Set-Cookie: test_cookie=1; expires={} GMT; Max-Age=86400\r\n'
        'Location: http://localhost:8000/cookies/resources/set-cookie-on-redirect.py?step=3\r\n\r\n'.format(expires.strftime('%a, %d-%b-%Y %H:%M:%S'))
    )
elif step == 3:
    cookies = {}
    if 'HTTP_COOKIE' in os.environ:
        header_cookies = os.environ['HTTP_COOKIE']
        header_cookies = header_cookies.split('; ')

        for cookie in header_cookies:
            cookie = cookie.split('=')
            cookies[cookie[0]] = cookie[1]

    sys.stdout.write('status: 200\r\n')
    if cookies.get('test_cookie', None) is not None:
        expires = datetime.utcnow() - timedelta(seconds=86400)
        sys.stdout.write(
            'Set-Cookie: text_cookie=deleted; expires={} GMT; Max-Age=0\r\n\r\n'
            'PASSED: Cookie successfully set\n'.format(expires.strftime('%a, %d-%b-%Y %H:%M:%S'))
        )
    else:
        sys.stdout.write('\r\nFAILED: Cookie not set\n')

    sys.stdout.write(
        '<script src=\'util.js\'></script>'
        '<script>if (window.testRunner && window.internals) setEnableFeature(false, finishJSTest);</script>'
    )
else:
    sys.stdout.write('\r\n')