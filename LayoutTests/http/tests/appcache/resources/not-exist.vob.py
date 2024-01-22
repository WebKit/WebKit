#!/usr/bin/env python3

import sys
import time

time.sleep(5)

sys.stdout.write(
    'status: 404\r\n'
    'Content-Type: text/html\r\n\r\n'
    'File not found\n'
)