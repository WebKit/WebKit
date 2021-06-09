#!/usr/bin/env python3

import os
import sys
import time
from urllib.parse import parse_qs

http_origin = os.environ.get('HTTP_ORIGIN', '')

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
name = query.get('name', ['abe.png'])[0]
contentType = query.get('contentType', ['image/png'])[0]

delay = True if query.get('delay', [None])[0] is not None else False
origin = True if query.get('origin', [None])[0] is not None else False
allowCreds = True if query.get('allowCredentials', [None])[0] is not None else False
allowCache = True if query.get('allowCache', [None])[0] is not None else False

if delay:
    time.sleep(0.000001 * int(query.get('delay', [0])[0]))

if origin:
    sys.stdout.write('Access-Control-Allow-Origin: {}\r\n'.format(query.get('origin', [''])[0]))
elif http_origin:
    sys.stdout.write(
        'Access-Control-Allow-Origin: {}\r\n'
        'Vary: Origin\r\n'.format(http_origin)
    )

if allowCreds:
    sys.stdout.write('Access-Control-Allow-Credentials: true\r\n')

if allowCache:
    sys.stdout.write('Cache-Control: max-age=100\r\n')

sys.stdout.write(
    'Content-Length: {}\r\n'
    'Content-Type: {}\r\n\r\n'.format(os.path.getsize(os.path.join(os.path.dirname(__file__), name)), contentType)
)
sys.stdout.flush()
with open(os.path.join(os.path.dirname(__file__), name), 'rb') as file:
    sys.stdout.buffer.write(file.read())
