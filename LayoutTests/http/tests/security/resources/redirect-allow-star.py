#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
url = query.get('url', [''])[0]
code = int(query.get('code', [302])[0])

sys.stdout.write(
    'status: {}\r\n'
    'Location: {}\r\n'
    'Access-Control-Allow-Origin: *\r\n'
    'Cache-Control: no-store\r\n'
    'Content-Type: text/html\r\n\r\n'.format(code, url)
)