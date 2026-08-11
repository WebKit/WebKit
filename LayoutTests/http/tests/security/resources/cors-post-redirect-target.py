#!/usr/bin/env python3

import os
import sys

origin = os.environ.get('HTTP_ORIGIN', None)

sys.stdout.write(
    'Cache-Control: no-store\r\n'
    'Content-Type: text/html\r\n\r\n'
)

if origin:
    sys.stdout.write('Origin header value: {}'.format(origin))
else:
    sys.stdout.write('There was no origin header')

sys.stdout.write(
    '<script>\n'
    'if (window.testRunner)\n'
    '    testRunner.notifyDone();\n'
    '</script>\n'
)