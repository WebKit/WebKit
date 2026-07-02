#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

content_type = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('m', ['text/html'])[0]

sys.stdout.write(
    'status: 200\r\n'
    'Content-Type: {}\r\n\r\n'
    '<h1>Hello</h1>'.format(content_type)
)