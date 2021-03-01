#!/usr/bin/env python3

import base64
import os
import sys

credentials = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')
username = credentials[0]
password = ':'.join(credentials[1:])

sys.stdout.write('Content-Type: text/html\r\n\r\n')

if not username:
    sys.stdout.write('No HTTP authentication credentials<br>')
else:
    sys.stdout.write('Authenticated as {} with password {}<br>'.format(username, password))

sys.stdout.write(
    '<script>\n'
    'if (window.testRunner)\n'
    '    testRunner.notifyDone();\n'
    '</script>\n'
)