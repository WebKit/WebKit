#!/usr/bin/env python3

import base64
import cgi
import os
import sys

credentials = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')
username = credentials[0]
password = ':'.join(credentials[1:])

sys.stdout.write(
    'Cache-Control: no-store\r\n'
    'Connection: close\r\n'
    'Content-Type: text/html\r\n'
)

if not username or not password:
    sys.stdout.write(
        'WWW-Authenticate: Basic realm=\"TestRealm\"\r\n'
        'status: 401\r\n\r\n'
        'Please send a username and password'
    )
else:
    form = cgi.FieldStorage()
    sys.stdout.write(
        '\r\n'
        'Uploaded blob data: {}'.format(str(form.getvalue('data'), 'utf-8'))
    )