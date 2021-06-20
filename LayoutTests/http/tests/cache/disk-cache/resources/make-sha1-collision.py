#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

path = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('path', [''])[0]

sys.stdout.write(
    'status: 200\r\n'
    'Cache-control: max-age=10000\r\n'
    'Content-Type: application/pdf\r\n\r\n'
)

sys.stdout.flush()
with open(os.path.join(os.path.dirname(__file__), path), 'rb') as file:
    sys.stdout.buffer.write(file.read().replace(b'SVN is the best!', b'SHA-1 is dead!!!'))
