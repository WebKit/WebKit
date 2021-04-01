#!/usr/bin/env python3

import sys
import time

time.sleep(1000)

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    'test for applicationCache.abort'
)