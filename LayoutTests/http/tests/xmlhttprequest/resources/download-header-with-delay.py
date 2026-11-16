#!/usr/bin/env python3
import os
import sys
import time

from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''))
delay = int(query.get('delay', [0])[0])
time.sleep(delay / 1000)

sys.stdout.write(
    'Content-Type: application/x-no-buffering-please\r\n'
    'Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n'
    'Cache-Control: no-cache, no-store, must-revalidate\r\n'
    'Pragma: no-cache\r\n'
    'Content-Type: text/html\r\n'
    '\r\n'
    '{delay}\n'
    'foobar\n'.format(delay=delay)
)

sys.stdout.flush()
