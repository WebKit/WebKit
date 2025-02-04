#!/usr/bin/env python3

import os
import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

modified_since = os.environ.get('HTTP_IF_MODIFIED_SINCE', '')

sys.stdout.write('Content-Type: text/html\r\n')

if modified_since:
    sys.stdout.write('status: 304\r\n\r\n')
    sys.exit(0)

sys.stdout.write(
    'Cache-Control: no-cache\r\n'
    'Etag: 123456789\r\n\r\n'
    'body text'
)
