#!/usr/bin/env python3
import os
import sys

if os.environ.get('HTTP_AUTHORIZATION'):
    import authenticate

else:
    sys.stdout.write(
        'Content-Type: text/plain\r\n'
        'status: 401\r\n'
        'WWW-Authenticate: Basic realm="WebKit Test Area"\r\n'
        '\r\n'
    )
