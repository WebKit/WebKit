#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Length: 16\r\n'
    'X-Content-Type-Options: nosniff\r\n'
    'Content-Type: application/octet-stream\r\n\r\n'

    '\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f'
)
