#!/usr/bin/env python3

import os
import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

https = os.environ.get('HTTPS')

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<script>\n'
    'if (window.testRunner)\n'
    '    testRunner.dumpAsText();\n'
    '</script>\n'
    'HTTPS is {}!\n'.format(https)
)

