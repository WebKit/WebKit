#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
script_url = os.environ.get('SCRIPT_URL')
uri = '/'.join(script_url.split('/')[0:-1])
host = os.environ.get('HTTP_HOST')
status = int(query.get('status', [200])[0])
target = query.get('target', [''])[0]

sys.stdout.write('Content-Type: text/html\r\n\r\n')

if status in [301, 302, 303, 307]:
    sys.stdout.write(
        'status: {status}\r\n'
        'Location: http://{host}{uri}\r\n\r\n'.format(status=status, host=host, uri=uri)
    )
else:
    sys.stdout.write(
        'status: 500\r\n\r\n'
        'Unexpected status code ({}) received.'.format(status)
    )