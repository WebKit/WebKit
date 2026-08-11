#!/usr/bin/env python3
import os
import sys

from urllib.parse import unquote, unquote_to_bytes

query = os.environ.get('QUERY_STRING', '')
split_query = {}
for pair in query.split('&'):
    split_query[unquote(pair.split('=')[0])] = '='.join(pair.split('=')[1:])

typ = unquote(split_query.get('type', 'text/html'))

sys.stdout.write(
    'Content-Type: {}\r\n'
    'status: 200\r\n'
    '\r\n'.format(typ)
)

sys.stdout.flush()
os.write(sys.stdout.fileno(), unquote_to_bytes(split_query.get('content')))
