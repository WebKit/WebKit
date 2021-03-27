#!/usr/bin/env python3

import os
import sys

purpose = os.environ.get('HTTP_PURPOSE', '')

sys.stdout.write('Content-Type: text/html\r\n\r\n')

if purpose == 'prefetch':
    sys.stdout.write(
        '<script>\n'
        'parent.window.postMessage(\'FAIL\', \'*\');\n'
        '</script>\n'
    )
    sys.exit(0)

sys.stdout.write(
    '<script>\n'
    'parent.window.postMessage(\'PASS\', \'*\');\n'
    '</script>\n'
)