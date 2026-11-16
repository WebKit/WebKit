#!/usr/bin/env python3

import os
import sys
import time
from urllib.parse import unquote

query_string = os.environ.get('QUERY_STRING', '')
query_arr = query_string.split('&')
query = {}
for part in query_arr:
    part = part.split('=')
    if len(part) > 1:
        query[part[0]] = []
        for sub_part in part[1:]:
            query[part[0]].append(unquote(sub_part))

delay = query.get('delay', [None])[0]
script = query.get('script', [''])[0]

if delay is not None:
    time.sleep(float(delay))
sys.stdout.write(
    'Cache-Control: no-cache, must-revalidate\r\n'
    'Content-Type: text/javascript\r\n\r\n{}'.format(script)
)