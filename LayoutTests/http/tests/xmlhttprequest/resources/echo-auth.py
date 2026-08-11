#!/usr/bin/env python3
import base64
import os
import sys

sys.stdout.write(
    'Content-Type: text/html\r\n'
    '\r\n'
)

credentials = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')
username = credentials[0]
password = ':'.join(credentials[1:])

if not username:
    sys.stdout.write('No authentication')
else:
    sys.stdout.write('User: {}, password: {}.\n'.format(username, password))
