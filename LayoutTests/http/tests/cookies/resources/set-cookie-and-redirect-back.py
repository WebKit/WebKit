#!/usr/bin/env python3

import os
import sys
from datetime import datetime, timedelta
from urllib.parse import parse_qs

redirect_back_to = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('redirectBackTo', [''])[0]

partitionedAttr = ""
if parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('isPartitioned', [None])[0]:
    partitionedSecureSameSiteAttr = "; Partitioned; Secure; SameSite=None"

sys.stdout.write('Content-Type: text/html\r\n')

if redirect_back_to:
    expires = datetime.utcnow() + timedelta(seconds=86400)
    sys.stdout.write(
        'status: 302\r\n'
        'Set-Cookie: test_cookie=1; expires={} GMT; Max-Age=86400{}\r\n'
        'Location: {}\r\n\r\n'.format(expires.strftime('%a, %d-%b-%Y %H:%M:%S'), partitionedSecureSameSiteAttr, redirect_back_to)
    )
else:
    sys.stdout.write(
        'status: 200\r\n\r\n'
        'FAILED: No redirectBackTo parameter found.\n'
    )