#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

origin = os.environ.get('HTTP_ORIGIN', '')
ident = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('id', ['id'])[0]

sys.stdout.write(
    'Cache-Control: no-store\r\n'
    'Content-Type: text/css\r\n'
)

if origin:
    sys.stdout.write(
        'Access-Control-Allow-Origin: {}\r\n\r\n'
        '.{} {{ background-color: yellow; }}'.format(origin, ident))
else:
    sys.stdout.write('\r\n.{} {{ background-color: blue; }}'.format(ident))