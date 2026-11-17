#!/usr/bin/env python3

import os
import sys

sys.stdout.write(
    'Content-Type: text/plain\r\n'
    'Cache-Control: max-age=3600\r\n'
    'Age: 0\r\n\r\n'
)
sys.stdout.flush()
sys.stdout.buffer.write(sys.stdin.buffer.read(int(os.environ.get('CONTENT_LENGTH') or 0)))
