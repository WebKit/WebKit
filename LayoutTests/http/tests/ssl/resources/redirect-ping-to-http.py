#!/usr/bin/env python3

import sys

sys.stdout.write(
    'status: 303\r\n'
    'Location: http://127.0.0.1:8000/ping-target-does-not-exist\r\n'
    'Content-Type: text/html\r\n\r\n'
)