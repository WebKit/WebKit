#!/usr/bin/env python3

import os
import sys

sys.stdout.write(
    'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
    'Access-Control-Allow-Credentials: true\r\n'
    'Content-Type: image/png\r\n\r\n'
)

sys.stdout.flush()
with open(os.path.join(os.path.dirname(__file__), 'abe.png'), 'rb') as file:
    sys.stdout.buffer.write(file.read())
