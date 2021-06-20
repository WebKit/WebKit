#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

allowCache = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('allowCache', [''])[0]

if allowCache:
    sys.stdout.write('Cache-Control: max-age=100\r\n')
sys.stdout.write(
    'Access-Control-Allow-Origin: *\r\n'
    'Content-Type: image/png\r\n\r\n'
)

sys.stdout.flush()
with open(os.path.join(os.path.dirname(__file__), 'abe.png'), 'rb') as file:
    sys.stdout.buffer.write(file.read())
