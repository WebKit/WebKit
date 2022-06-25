#!/usr/bin/env python3

import os
import sys

https = os.environ.get('HTTPS')

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<script>\n'
    'if (window.testRunner)\n'
    '    testRunner.dumpAsText();\n'
    '</script>\n'
    'HTTPS is {}!\n'.format(https)
)

