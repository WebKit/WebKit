#!/usr/bin/env python3

import sys
import time

sys.stdout.write(
    'Content-Type: text/html\r\n'
    '\r\n'
)
sys.stdout.flush()

time.sleep(5)

sys.stdout.write('<p>This same-site load should never commit.</p>')
sys.stdout.flush()
