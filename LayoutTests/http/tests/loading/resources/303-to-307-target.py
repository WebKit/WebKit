#!/usr/bin/env python3

import sys

sys.stdout.write(
    'status: 307\r\n'
    'Location: 307-post-output-target.py\r\n'
    'Content-Type: text/html\r\n\r\n'
)