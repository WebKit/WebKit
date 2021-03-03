#!/usr/bin/env python3

import base64
import os
import sys

username = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')[0]

sys.stdout.write('Content-Type: text/html\r\n')

if not username:
    sys.stdout.write(
        'WWW-Authenticate: Basic realm="Please press Cancel"\r\n'
        'status: 401\r\n\r\n'
        '<script>\n'
        '   if (window.testRunner)\n'
        '       testRunner.dumpAsText();\n'
        '</script>\n'
        'PASS\n'
    )
else:
    sys.stdout.write('\r\nFAIL: Why do you have credentials?')