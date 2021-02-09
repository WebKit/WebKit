#!/usr/bin/env python3

import os
import sys

status = os.environ.get('HTTP_X_STATUS_CODE_NEEDED')

if status == '304':
    sys.stdout.write(
        'Content-Type: text/html\r\n'
        'status: 304\r\n'
        '\r\n'
    )
else:
    sys.stdout.write(
        'Content-Type: text/plain\r\n'
        'Cache-Control: public, max-age=31556926\r\n'
        '\r\n'
    )

    sys.stdout.write('Plain text resource.')
