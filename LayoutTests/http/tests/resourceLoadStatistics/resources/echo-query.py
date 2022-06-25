#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

value = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('value', [None])[0]

sys.stdout.write(
    'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
    'Access-Control-Allow-Headers: X-WebKit\r\n'
    'Content-Type: text/html\r\n\r\n'
)

if value:
    sys.stdout.write(value)
else:
    sys.stdout.write('No query parameter named \'value.\'')