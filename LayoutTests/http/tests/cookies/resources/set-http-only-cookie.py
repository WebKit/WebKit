#!/usr/bin/env python3

import os
import sys
from datetime import datetime, timedelta
from urllib.parse import parse_qs

cookie_name = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('cookieName', [''])[0]
expires = datetime.utcnow() + timedelta(seconds=86400)

sys.stdout.write('Content-Type: text/html\r\n')

if cookie_name:
    sys.stdout.write(
        'Set-Cookie: {}=1; expires={} GMT; Max-Age=86400; path=/; HttpOnly\r\n\r\n'
        'Yeah'.format(cookie_name, expires.strftime('%a, %d-%b-%Y %H:%M:%S'))
    )
else:
    sys.stodut.write(
        'status: 200\r\n\r\n'
        'FAILED: No cookieName parameter found.\n'
    )