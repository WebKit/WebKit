#!/usr/bin/env python3

import sys

sys.stdout.write(
    'status: 307\r\n'
    'Location: http://localhost:8000/appcache/resources/identifier-test-real.py\r\n'
    'Content-Type: text/html\r\n\r\n'
)