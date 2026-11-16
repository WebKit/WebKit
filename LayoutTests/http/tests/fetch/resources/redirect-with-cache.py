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
    'Content-Type: text/html\r\n'.format(code, url)
)

if 'ENABLECACHING' in os.environ.keys() or 'enableCaching' in query.keys():
    sys.stdout.write('Cache-Control: max-age=31536000\r\n\r\n')
else:
    sys.stdout.write('Cache-Control: no-store\r\n\r\n')