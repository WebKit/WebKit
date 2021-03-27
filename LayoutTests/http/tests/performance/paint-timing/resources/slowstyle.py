#!/usr/bin/env python3

import os
import sys
import time
from urllib.parse import parse_qs

delay = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('delay', [None])[0]

if delay is not None:
    time.sleep(float(delay))
sys.stdout.write(
    'status: 302\r\n'
    'Cache-Control: no-cache, must-revalidate\r\n'
    'Location: style.css\r\n'
    'Content-Type: text/html\r\n\r\n'
)