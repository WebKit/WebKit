#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
allow_origin = query.get('allow', [''])[0]

if allow_origin == 'true':
    sys.stdout.write('Access-Control-Allow-Origin: *\r\n')
sys.stdout.write('Content-Type: image/png\r\n\r\n')

sys.stdout.flush()
with open(os.path.join(os.path.dirname(__file__), query.get('file', [''])[0]), 'rb') as file:
    sys.stdout.buffer.write(file.read())
