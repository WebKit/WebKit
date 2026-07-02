#!/usr/bin/env python3
import sys

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Location: infinite-loop.py\r\n'
    'status: 302\r\n'
    '\r\n'
)
