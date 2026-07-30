#!/usr/bin/env python3

import base64
import os
import sys

module = base64.b64decode(
    'AGFzbQEAAAABBwFgAn9/AX8DAgEABQMBAAEHEAIDYWRkAAAGbWVtb3J5AgAKCQEHACAAIAFqCwAaBG5hbWUAExJlbWJlZGRlZC1uYW1lLndhc20='
)

headers = b'Content-Type: application/wasm\r\n'
if os.environ.get('QUERY_STRING') == 'memory-cache':
    headers += b'Cache-Control: max-age=3600\r\n'

sys.stdout.buffer.write(headers + b'\r\n' + module)
