#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
content = query.get('content', [''])[0]
repeat = int(query.get('repeat', ['1'])[0])

sys.stdout.write(
    'Content-Type: text/plain; charset=utf-8\r\n'
    'Cache-Control: no-store\r\n'
    '\r\n'
)
sys.stdout.flush()
sys.stdout.buffer.write((content * repeat).encode('utf-8'))
