#!/usr/bin/env python3

import os
import sys
from random import randint
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
ident = query.get('id', [''])[0]

max_age = 12 * 31 * 24 * 60 * 60
random_ident = True if query.get('random_id', [None])[0] is not None else False

sys.stdout.write(
    f'Cache-Control: public, max-age={max_age}\r\n'
    'Content-Type: text/html\r\n\r\n'
)

if not random_ident:
    ident = ''
    charset = 'ABCDEFGHIKLMNOPQRSTUVWXYZ0123456789'
    for i in range(0, 16):
        ident += charset[randint(0, len(charset) - 1)]

sys.stdout.write(f'<html><body>Some random text{ident}</hml></body>')