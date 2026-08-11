#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
status = int(query.get('status', [500])[0])
target = query.get('target', [''])[0]

uri = '{}/{}'.format('/'.join(os.environ.get('SCRIPT_URL', '').split('/')[:-1]), target)

sys.stdout.write('Content-Type: text/html\r\n')

host = os.environ.get('HTTP_HOST', '')
if query.get('host', [None])[0] is not None:
    host = query.get('host')[0]

if status in [301, 302, 303, 307, 308]:
    sys.stdout.write(
        'status: {}\r\n'
        'Location: http://{}{}\r\n\r\n'.format(status, host, uri)
    )
else:
    sys.stdout.write(
        'status: 500\r\n\r\n'
        'Unexpected status code ({}) received.'.format(status)
    )
