#!/usr/bin/env python3

import os
import sys
from save_ping import save_ping

query_string = os.environ.get('QUERY_STRING', None)
if query_string is not None:
    query_string = '?{}'.format(query_string)

save_ping(True)

sys.stdout.write(
    'status: 307\r\n'
    'Location: save-ping.py{}\r\n'
    'Content-Type: text/html\r\n\r\n'.format(query_string)
)