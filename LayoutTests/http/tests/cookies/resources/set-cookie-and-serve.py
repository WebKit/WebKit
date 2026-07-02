#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
cookie_name = query.get('cookie-name', [''])[0]
cookie_value = query.get('cookie-value', [''])[0]
destination = query.get('destination', [''])[0]
cookie_count = query.get('cookie-count', [1])[0]

sys.stdout.buffer.write(
    'Content-Type: text/html\r\n'
    'Cache-Control: no-store\r\n'
    'Set-Cookie: {}={}; path=/\r\n'.format(cookie_name, cookie_value).encode()
)

content = '<script>for (let i = 0; i < {}; i++) document.cookie=`{}_js_${{i}}={}`;</script>'.format(cookie_count, cookie_name, cookie_value).encode()
if len(destination) != 0:
    with open(os.path.join(os.path.dirname(__file__), destination), 'rb') as file:
        content = file.read()

sys.stdout.buffer.write('Content-Length: {}\r\n\r\n'.format(len(content)).encode())
sys.stdout.buffer.write(content)

sys.exit(0)
