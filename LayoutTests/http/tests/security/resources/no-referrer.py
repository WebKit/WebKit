#!/usr/bin/env python3

import os
import sys

referer = os.environ.get('HTTP_REFERER', '')

sys.stdout.write(
    'Cache: no-cache, no-store\r\n'
    'Content-Type: text/html\r\n\r\n'
)

if referer:
    print('log(\'External script (HTTP Referer): FAIL\');')
else:
    print('log(\'External script (HTTP Referer): PASS\');')

sys.stdout.write(
    'if (document.referrer.toString() != "")\n'
    '    log(\'External script (JavaScript): FAIL\');\n'
    'else\n'
    '    log(\'External script (JavaScript): PASS\');\n'
)