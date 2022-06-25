#!/usr/bin/env python3

import os
import sys

match = os.environ.get('HTTP_IF_NONE_MATCH', '')
if match == 'foo':
    sys.stdout.write(
        'status: 304\r\n'
        'ETag: foo\r\n'
        'Content-Type: text/html\r\n\r\n'
    )
    sys.exit(0)

sys.stdout.write(
    'Content-Type: text/css\r\n'
    'ETag: foo\r\n'
    'Cache-Control: max-age=0\r\n\r\n'
    '.testClass\n'
    '{\n'
    '    color: green;\n'
    '}\n'
)