#!/usr/bin/env python3

import os
import sys
import time
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
delay_ms = query.get('delay_ms', [None])[0]
conversion_data = query.get('conversionData', [None])[0]
priority = query.get('priority', [None])[0]

if delay_ms is not None:
    time.sleep(int(delay_ms) * 0.001)

sys.stdout.write(
    'status: 302\r\n'
    'Cache-Control: no-cache, no-store, must-revalidate\r\n'
    'Access-Control-Allow-Origin: *\r\n'
    'Access-Control-Allow-Methods: GET\r\n'
    'Content-Type: text/html\r\n'
)

if conversion_data is not None and priority is not None:
    sys.stdout.write('Location: /.well-known/private-click-measurement/trigger-attribution/{}/{}?attributionSource=https://127.0.0.1\r\n'.format(conversion_data, priority))
elif conversion_data is not None:
    sys.stdout.write('Location: /.well-known/private-click-measurement/trigger-attribution/{}?attributionSource=https://127.0.0.1\r\n'.format(conversion_data))

sys.stdout.write('\r\n')
