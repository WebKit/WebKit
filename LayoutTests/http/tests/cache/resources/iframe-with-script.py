#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
script_cache_control = query.get('script-cache-control', [''])[0]

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Cache-Control: no-cache\r\n\r\n'
    f"<script src='cache-simulator.py?uniqueId=1&Cache-control={script_cache_control}'></script>\n"
)
