#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

sys.stdout.write('Content-Type: text/html\r\n')

users = parse_qs(os.environ.get('QUERY_STRING', '')).get('users', [''])[0].split(',')
users = filter(lambda s: len(s), users)

sys.stdout.write(
    'Content-Type: text/event-stream\r\n'
    'Connection: keep-alive\r\n'
    'Cache-Control: no-cache\r\n'
    'status: 200\r\n'
    '\r\n'
)

for user in users:
    sys.stdout.write(
        'event: user\n'
        'data: {}\n'
        '\n'.format(user)
    )

sys.stdout.write(
    'data: the end.\n'
    '\n'
)
