#!/usr/bin/env python3
import os
import sys

sys.stdout.write('Content-Type: text/html\r\n')

if os.environ.get('HTTP_ORIGIN'):
    sys.stdout.write(
        'Access-Control-Allow-Origin: *\r\n'
        'Access-Control-Allow-Headers: X-Proprietary-Header\r\n'
        '\r\n'
        'PASS: Origin header correctly sent'
    )
else:
    sys.stdout.write(
        '\r\n'
        'FAIL: No origin header sent'
    )
