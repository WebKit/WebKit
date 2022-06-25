#!/usr/bin/env python3
import base64
import os
import sys

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Set-Cookie: WK-cross-origin=1\r\n'
    'Cache-Control: no-store\r\n'
    'Last-Modified: Thu, 19 Mar 2009 11:22:11 GMT\r\n'
    'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
    'Access-Control-Allow-Credentials: true\r\n'
    'Connection: close\r\n'
)

if os.environ.get('HTTP_AUTHORIZATION'):
    sys.stdout.write('\r\n')
    username, _ = base64.b64decode(os.environ['HTTP_AUTHORIZATION'].split(' ')[1]).split(b':')
    sys.stdout.write("log('PASS: Loaded, user {}');\n".format(username.decode('utf-8')))

else:
    sys.stdout.write(
        'WWW-Authenticate: Basic realm="WebKit xmlhttprequest/cross-origin-no-authorization"\r\n'
        'status: 401\r\n'
        '\r\n'
        'Authentication canceled'
    )

sys.stdout.flush()
