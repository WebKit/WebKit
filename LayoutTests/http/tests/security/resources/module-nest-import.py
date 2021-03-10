#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

url = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('url', [''])[0]

sys.stdout.write(
    'Cache-Control: no-store\r\n'
    'Connection: close\r\n'
    'Content-Type: application/javascript\r\n\r\n'
    'import "{}"'.format(url)
)