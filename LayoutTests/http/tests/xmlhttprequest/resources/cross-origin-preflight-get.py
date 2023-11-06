#!/usr/bin/env python3
import os
import sys

sys.stdout.buffer.write(b'Content-Type: text/html\r\n')

if os.environ.get('HTTP_ORIGIN'):
    sys.stdout.buffer.write(
        b'Access-Control-Allow-Origin: *\r\n'
        b'Access-Control-Allow-Headers: X-Proprietary-Header\r\n'
        b'\r\n'
        b'PASS: Origin header correctly sent'
    )
else:
    sys.stdout.buffer.write(
        b'\r\n'
        b'FAIL: No origin header sent'
    )
