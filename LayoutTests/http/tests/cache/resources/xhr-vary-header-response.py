#!/usr/bin/env python3

import os
import sys

origin = True if os.environ.get('HTTP_ORIGIN', None) is not None else False

sys.stdout.write(
    'Vary: Origin\r\n'
    'Cache-Control: max-age=31536000\r\n'
    'Content-Type: text/html\r\n'
)

if origin:
    sys.stdout.write(
        'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n\r\n'
        'Cross origin '
    )
else:
    sys.stdout.write('\r\nSame origin ')

sys.stdout.write('response')