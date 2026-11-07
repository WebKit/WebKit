#!/usr/bin/env python3

import os
import sys

sys.stdout.write(
    'Content-Type: text/plain\r\n'
    'Cache-Control: no-store\r\n\r\n'
)
sys.stdout.flush()
sys.stdout.buffer.write(sys.stdin.buffer.read(int(os.environ.get('CONTENT_LENGTH') or 0)))
