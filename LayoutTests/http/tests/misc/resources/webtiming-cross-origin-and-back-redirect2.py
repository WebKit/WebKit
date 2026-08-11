#!/usr/bin/env python3

import sys

# 127.0.0.1 is where the test originally started. We redirected to "localhost" before this.
sys.stdout.write(
    'Location: http://127.0.0.1:8000/misc/resources/webtiming-cross-origin-and-back2.html\r\n'
    'status: 302\r\n'
    'Content-Type: text/html\r\n\r\n'
)