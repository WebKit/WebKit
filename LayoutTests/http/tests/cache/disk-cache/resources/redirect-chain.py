#!/usr/bin/env python3

import os
import sys
from random import randint
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
length = int(query.get('length', [0])[0])
unique_id = query.get('uniqueId', [''])[0]

sys.stdout.write(
    'Cache-Control: max-age=100\r\n'
    'Content-Type: text/html\r\n'
)

if length == 0:
    sys.stdout.write('status: 200\r\n\r\n')
    sys.exit(0)

length -= 1
sys.stdout.write(
    'status: 302\r\n'
    f'Location: redirect-chain.py?length={length}&uniqueId={unique_id}&random={randint(0, sys.maxsize)}\r\n\r\n'
)