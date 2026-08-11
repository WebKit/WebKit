#!/usr/bin/env python3

import os
import sys
from random import randint
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
cache_control = query.get('cache_control', [''])[0]
code = query.get('code', [302])[-1]
expiration = query.get('expiration', [None])[0]
random_id = query.get('random_id', [None])[0]
url = query.get('url', [''])[0]

sys.stdout.write('status: {}\r\n'.format(int(code)))

if random_id is not None:
    ident = ''
    charset = 'ABCDEFGHIKLMNOPQRSTUVWXYZ0123456789'
    for _ in range(0, 16):
        ident += charset[randint(0, len(charset) - 1)]
    sys.stdout.write('Location: {}?id={}\r\n'.format(url, ident))
else:
    sys.stdout.write('Location: {}\r\n'.format(url))

if expiration is not None:
    expires = '{} +0000'.format(expires.strftime('%a, %d %b %Y %H:%M:%S'))
    sys.stdout.write('Expires: {}\r\n'.format(expires))

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Cache-Control: {}\r\n\r\n'.format(cache_control)
)