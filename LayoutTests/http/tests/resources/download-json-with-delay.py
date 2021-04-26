#!/usr/bin/env python3

import os
import sys
import time
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
allow_cache = query.get('allowCache', [None])[0]
cors = query.get('cors', [None])[0]

sys.stdout.write('Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n')

if allow_cache is not None:
    sys.stdout.write('Content-Type: application/json\r\n')
else:
    sys.stdout.write(
        'Content-Type: application/x-no-buffering-please\r\n'
        'Cache-Control: no-cache, no-store, must-revalidate\r\n'
        'Pragma: no-cache\r\n'
    )

if cors is not None:
    sys.stdout.write('Access-Control-Allow-Origin: *\r\n')

sys.stdout.write('\r\n')

iteration = int(query.get('iteration', [0])[0])
delay = int(query.get('delay', [0])[0])

sys.stdout.write('[{}, {} '.format(iteration, delay))

for i in range(1, iteration + 1):
    sys.stdout.write(', {}, "foobar"'.format(i))
    sys.stdout.flush()
    time.sleep(delay / 1000)

sys.stdout.write(']')
