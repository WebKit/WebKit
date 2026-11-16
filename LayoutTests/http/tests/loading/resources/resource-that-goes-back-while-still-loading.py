#!/usr/bin/env python3

import sys
import time

sys.stdout.write(
    'status: 200\r\n'
    'Content-Type: text/html\r\n\r\n'
)

sys.stdout.flush()

sys.stdout.write(' ' * 1024)
sys.stdout.flush()

sys.stdout.write(
    'This page should trigger a back navigation while it\'s still loading.<br>\r\n'
    '<script>setTimeout(\'window.history.back()\', 0);</script>'
)

while True:
    sys.stdout.write('Still loading...<br>\r\n')
    sys.stdout.flush()
    time.sleep(1)