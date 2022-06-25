#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs, urlencode

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
required_referer = query.get('referer', [''])[0]
refer_header = os.environ.get('HTTP_REFERER', None)

if refer_header is None or required_referer not in refer_header:
    sys.stdout.write('Content-Type: text/html\r\n\r\n')
    sys.exit(0)

filename = query.get('name', [''])[0]
typ = query.get('type', [''])[0]
os.environ['QUERY_STRING'] = urlencode({'name': filename, 'type': typ})

from serve_video import serve_video
