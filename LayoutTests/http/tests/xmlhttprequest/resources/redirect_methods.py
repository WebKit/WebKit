#!/usr/bin/env python3
import os
import sys

from urllib.parse import parse_qs
query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'status: {code}\r\n'
    'Location: {url}\r\n'
    '\r\n'.format(
        code=query.get('code', [200])[0],
        url=query.get('url', [''])[0],
))
