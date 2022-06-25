#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
name = query.get('name', [''])[0]
value = query.get('value', [''])[0]

sys.stdout.write(
    'Set-Cookie: {name}={value}; path=/\r\n'
    'Content-Type: text/html\r\n\r\n'
    'Set {name}={value}\n'.format(name=name, value=value)
)