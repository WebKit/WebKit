#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

request_method = os.environ.get('REQUEST_METHOD')

sys.stdout.write('Content-Type: text/event-stream\r\n')

if request_method == 'OPTIONS':
    sys.stdout.write('\r\nGot unexpected preflight request\n')
    sys.exit(0)

count = int(parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('count', [0])[0])
http_origin = os.environ.get('HTTP_ORIGIN')
http_last_event_id = os.environ.get('HTTP_LAST_EVENT_ID')

if count == 1 or count == 2:
    sys.stdout.write('Access-Control-Allow-Origin: *\r\n')
elif count > 2:
    sys.stdout.write('Access-Control-Allow-Origin: {}\r\n'.format(http_origin))

if count == 2 or count > 3:
    sys.stdout.write('Access-Control-Allow-Credentials: true\r\n')

sys.stdout.write('\r\n')

if http_last_event_id != '77':
    print('id: 77\ndata: DATA1\nretry: 0\n')
else:
    print('data: DATA2\n')