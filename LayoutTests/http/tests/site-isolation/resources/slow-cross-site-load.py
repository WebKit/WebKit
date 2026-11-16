#!/usr/bin/env python3

import sys
import time

sys.stdout.write(
    'Content-Type: text/html\r\n'
    '\r\n'
)
sys.stdout.flush()

# Keep the cross-site provisional load pending long enough for the test to start
# a second, same-site navigation in the original process.
time.sleep(5)

sys.stdout.write('<p>This cross-site load should never commit.</p>')
sys.stdout.flush()
