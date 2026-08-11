#!/usr/bin/env python3

import sys
import time

time.sleep(60)

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    'FAIL: Should never see this'
)