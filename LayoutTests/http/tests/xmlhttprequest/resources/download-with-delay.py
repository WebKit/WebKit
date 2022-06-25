#!/usr/bin/env python3
import os
import sys
import time

from urllib.parse import parse_qs

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n'
    'Cache-Control: no-cache, no-store, must-revalidate\r\n'
    'Content-Type: application/x-no-buffering-please\r\n'
    '\r\n'
)

query = parse_qs(os.environ.get('QUERY_STRING', ''))
iteration = int(query.get('iteration', [1])[0])
delay = int(query.get('delay', [0])[0])

print(iteration)
print(delay)

for i in range(iteration):
    print(i + 1)
    print('foobar')
    sys.stdout.flush()
    time.sleep(delay / 1000)

