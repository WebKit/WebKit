#!/usr/bin/env python3

import os
import sys

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<html>\n'
    '<head>\n'
    '<script>\n'
    'function runTest() {\n\n'
    '    if (window.testRunner)\n'
    '        testRunner.dumpAsText();\n\n'
)

if os.environ.get('HTTP_REFERER'):
    sys.stdout.write('document.write("FAIL: The server should not receive a referrer which is not set by user agent.");')
else:
    sys.stdout.write('document.write("PASS: The server didn\'t receive a referrer.");')

sys.stdout.write(
    '}\n'
    '</script>\n'
    '</head>\n'
    '<body onload="runTest()">\n'
    '</body>\n'
    '</html>\n'
)