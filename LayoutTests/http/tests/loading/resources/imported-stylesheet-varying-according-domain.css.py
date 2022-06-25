#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

host = os.environ.get('HTTP_HOST', '')
domain = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('domain', [''])[0]

color = 'green' if host == domain else 'red'

sys.stdout.write(
    'Access-Control-Allow-Origin: *\r\n'
    'Content-Type: text/css\r\n\r\n'
    'body {{ background-color: {}; }}'.format(color)
)