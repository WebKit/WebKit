#!/usr/bin/env python3

import os
import sys
from save_ping import save_ping

query_string = os.environ.get('QUERY_STRING', '')
if query_string != '':
    query_string = '?{}'.format(query_string)

sys.stdout.write(
    'status: 307\r\n'
    'Location: save-ping.py{}\r\n'.format(query_string)
)

save_ping(True)
