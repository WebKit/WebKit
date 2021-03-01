#!/usr/bin/env python3
import json
import os
import sys

sys.stdout.write(
    'Content-Type: text/plain\r\n'
    'Cache-Control: no-store\r\n'
    '\r\n'
)

headers = {}
for headername, headervalue in os.environ.items():
    if not headername.startswith('HTTP_'):
        continue
    headers[headername[5:].lower().replace('_', '-')] = headervalue

sys.stdout.write(json.dumps(headers))

