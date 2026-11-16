#!/usr/bin/env python3

import base64
import os
import sys
from urllib.parse import parse_qs, urlencode

username = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')[0]
query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)

sys.stdout.write(
    'Cache-Control: no-store\r\n'
    'Connection: close\r\n'
)

if not username:
    sys.stdout.write(
        'WWW-authenticate: Basic realm="{}"\r\n'
        'status: 401\r\n\r\n'.format(os.environ.get('REQUEST_URI', ''))
    )
    sys.exit(0)

filename = query.get('name', ['404.txt'])[0]
typ = query.get('type', [''])[0]

os.environ['QUERY_STRING'] = urlencode({'name': filename, 'type': typ})

from serve_video import serve_video
