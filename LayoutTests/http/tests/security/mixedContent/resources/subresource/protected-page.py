#!/usr/bin/env python3

import base64
import os
import sys

username = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')[0]
uri = os.environ.get('REQUEST_URI', '')

sys.stdout.write(
    'Cache-Control: no-store\r\n'
    'Connection: close\r\n'
    'Content-Type: text/html\r\n'
)

if not username:
    sys.stdout.write(
        'WWW-authenticate: Basic realm="{}"\r\n'
        'status: 401\r\n'.format(uri)
    )

sys.stdout.write(
    '\r\n<!DOCTYPE html>\n'
    '<html>\n'
    '<body>\n'
    '<p>'
)

if username:
    sys.stdout.write('Authenticated with username {}.'.format(username))
else:
    sys.stdout.write('Unauthorized.')

sys.stdout.write(
    '</p>\n'
    '<script>\n'
    'if (window.testRunner)\n'
    '    testRunner.notifyDone();\n'
    '</script>\n'
    '</body>\n'
    '</html>\n'
)