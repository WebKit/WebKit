#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

location = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('location', [''])[0]

sys.stdout.write(
    'status: 301\r\n'
    'Location: {}\r\n'
    'Content-Type: text/html\r\n\r\n'.format(location)
)