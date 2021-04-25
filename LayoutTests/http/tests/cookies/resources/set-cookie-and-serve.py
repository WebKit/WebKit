#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
cookie_name = query.get('cookie-name', [''])[0]
cookie_value = query.get('cookie-value', [''])[0]
destination = query.get('destination', [''])[0]

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Cache-Control: no-store\r\n'
    'Set-Cookie: {}={}; path=/\r\n'.format(cookie_name, cookie_value)
)

with open(os.path.join(os.path.dirname(__file__), destination), 'r') as file:
    content = file.read()
    sys.stdout.write('Content-Length: {}\r\n\r\n{}'.format(len(content), content))

sys.exit(0)