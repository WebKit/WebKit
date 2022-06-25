#!/usr/bin/env python3

import sys
import time

time.sleep(1)

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    'CACHE MANIFEST\n'
    'abort-cache-onchecking.text'
)