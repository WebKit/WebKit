#!/usr/bin/env python3

import base64
import os
import sys

sys.stdout.write('Content-Type: text/html' + '\r\n')

expectedUsername = 'goodUsername'
expectedPassword = 'goodPassword'
realm = os.environ.get('REQUEST_URI')

credentials = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')
username = credentials[0]
password = ':'.join(credentials[1:])

if (username != expectedUsername) or (password != expectedPassword):
    sys.stdout.write(
        'WWW-Authenticate: Basic realm=\"{}\"\r\n'
        'status: 401\r\n'
        '\r\n'.format(realm)
        )

    sys.stdout.write('Bad username:password ({}:{})'.format(username, password))
else:
    sys.stdout.write('\r\n')