#!/usr/bin/env python3
import os
import sys

sys.stdout.write('Content-Type: text/html\r\n')

if os.environ.get('HTTP_ORIGIN'):
    sys.stdout.write(
        'Access-Control-Allow-Origin: null\r\n'
        '\r\n'
        'PASS'
    )

else:
    sys.stdout.write('\r\n')
