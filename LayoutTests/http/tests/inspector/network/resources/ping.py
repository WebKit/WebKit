#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

sys.stdout.write('Content-Type: text/html\r\n')

status = int(parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('status', [200])[0])

if status == 204:
    sys.stdout.write('status: 204\r\n\r\n')
elif status == 404:
    sys.stdout.write('status: 404\r\n\r\n')
else:
    sys.stdout.write(
        '\r\n'
        'Success'
    )