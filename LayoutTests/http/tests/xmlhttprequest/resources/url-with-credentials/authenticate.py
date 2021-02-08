#!/usr/bin/env python3
import base64
import os
import sys

sys.stdout.write('Content-Type: text/plain\r\n\r\n')

username, password = base64.b64decode(
    os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1],
).split(b':')
sys.stdout.write('User: {} Password: {}'.format(username.decode(), password.decode()))
