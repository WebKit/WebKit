#!/usr/bin/env python3

import sys
import time

time.sleep(0.0001)

sys.stdout.write(
    'status: 500\r\n'
    'Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n'
    'Cache-Control: no-cache, no-store, must-revalidate\r\n'
    'Pragma: no-cache\r\n'
    'Content-Type: text/css\r\n\r\n'
)

sys.stdout.flush()