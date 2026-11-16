#!/usr/bin/env python3

import os
import sys
from datetime import datetime, timedelta, timezone
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
name = query.get('name')[0]
value = query.get('value')[0]
secure = query.get('secure', [None])[0]
exp_time = datetime.now(timezone.utc) + timedelta(hours=1)

cookie = '{}={}; expires={} GMT; Max-Age=3600'.format(name, value, exp_time.strftime('%a, %d-%b-%Y %H:%M:%S'))
if secure:
    cookie += '; Secure'

sys.stdout.write(
    'Set-Cookie: {}\r\n'
    'Content-Type: text/html\r\n\r\n'.format(cookie)
)
