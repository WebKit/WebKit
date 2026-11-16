#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

status_code = int(parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('status-code', [200])[0])
script_url = os.environ.get('SCRIPT_URL', '')
http_host = os.environ.get('HTTP_HOST', '')

sys.stdout.write('status: {}\r\n'.format(status_code))

if status_code == 200:
    sys.stdout.write(
        'Content-Type: text/event-stream\r\n\r\n'
        'data: hello\n\n'
    )
elif status_code in [301, 302, 303, 307]:
    sys.stdout.write(
        'Content-Type: text/html\r\n'
        'Location: http://{}{}/simple-event-stream.asis\r\n\r\n'.format(http_host, '/'.join(script_url.split('/')[0:-1]))
    )
else:
    sys.stdout.write('Content-Type: text/html\r\n\r\n')