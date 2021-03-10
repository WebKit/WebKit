#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

url = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('url', [''])[0]

sys.stdout.write(
    'Location: {}\r\n'
    'status: 307\r\n'
    'Content-Type: text/html\r\n\r\n'.format(url)
)