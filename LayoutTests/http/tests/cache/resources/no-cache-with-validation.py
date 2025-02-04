#!/usr/bin/env python3

import os
import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

modified_since = os.environ.get('HTTP_IF_MODIFIED_SINCE', '')

sys.stdout.write('Content-Type: text/plain\r\n')

if modified_since:
    sys.stdout.write('status: 304\r\n\r\n')
    sys.exit(0)

sys.stdout.write(
    'Cache-control: no-cache\r\n'
    'Last-Modified: Thu, 01 Jan 1970 00:00:00 GMT\r\n\r\n'
    'foo\n'
)
