#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
redirect_url = query.get('redirectTo', [''])[0]

for name in ['name2', 'name3', 'message']:
    query_name = query.get(name, [None])[0]
    if query_name:
        redirect_url += '&{}={}'.format(name, query_name)

sys.stdout.write('Content-Type: text/html\r\n')
sys.stdout.write('Cache-Control: no-cache, no-store\r\n')
sys.stdout.write('Location: {}\r\n\r\n'.format(redirect_url))
sys.exit(0)