#!/usr/bin/env python3
import base64
import os
import sys

sys.stdout.buffer.write(
    b'Content-Type: text/html\r\n'
    b'Set-Cookie: WK-cross-origin=1\r\n'
    b'Cache-Control: no-store\r\n'
    b'Last-Modified: Thu, 19 Mar 2009 11:22:11 GMT\r\n'
    b'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
    b'Connection: close\r\n'
)

if os.environ.get('HTTP_AUTHORIZATION'):
    sys.stdout.buffer.write(b'\r\n')
    username, _ = base64.b64decode(os.environ['HTTP_AUTHORIZATION'].split(' ')[1]).split(b':')

    sys.stdout.buffer.write("log('PASS: Loaded, user {}');\n".format(username.decode('utf-8')).encode())

else:
    sys.stdout.buffer.write(
        b'WWW-Authenticate: Basic realm="WebKit xmlhttprequest/cross-origin-no-authorization"\r\n'
        b'status: 401\r\n'
        b'\r\n'
        b'Authentication canceled'
    )    
