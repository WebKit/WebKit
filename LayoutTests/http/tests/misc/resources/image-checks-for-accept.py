#!/usr/bin/env python3

import os
import sys

http_accept = os.environ.get('HTTP_ACCEPT', '')

if 'image/*' in http_accept:
    sys.stdout.write(
        'Content-Type: image/jpg\r\n'
        'Cache-Control: no-store\r\n'
        'Connection: close\r\n\r\n'
    )

    sys.stdout.flush()
    with open(os.path.join(os.path.dirname(__file__), 'compass.jpg'), 'rb') as fn:
        sys.stdout.buffer.write(fn.read())
else:
    sys.stdout.write('Content-Type: text/html\r\n\r\n')
