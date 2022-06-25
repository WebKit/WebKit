#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs, urlencode

user_agent = os.environ.get('HTTP_USER_AGENT', None)
query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)

if user_agent is None or 'WebKit/' not in user_agent or '(KHTML, like Gecko)' not in user_agent:
    sys.stdout.write('Content-Type: text/html\r\n\r\n')
    sys.exit(0)

filename = query.get('name', [''])[0]
typ = query.get('type', [''])[0]

os.environ['QUERY_STRING'] = urlencode({'name': filename, 'type': typ})

from serve_video import serve_video
