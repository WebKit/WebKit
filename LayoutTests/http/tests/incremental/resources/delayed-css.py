#!/usr/bin/env python3

import os
import sys
import time
from urllib.parse import parse_qs

delay = int(parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('delay')[0])

sys.stdout.write(
    'Cache-Control: no-cache, no-store\r\n'
    'Content-Type: text/css\r\n\r\n'
)

time.sleep(delay * 0.001)
print('.delayed { background-color: green !important }')