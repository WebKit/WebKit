#!/usr/bin/env python3

import sys
from datetime import datetime, timedelta

max_age = 12 * 31 * 24 * 60 * 60
expires = '{} +0000'.format((datetime.utcnow() + timedelta(seconds=max_age)).strftime('%a, %d %b %Y %H:%M:%S'))

sys.stdout.write(
    f'Cache-Control: public, max-age={max_age}\r\n'
    f'Expires: {expires}\r\n'
    'Content-Type: text/html\r\n\r\n'
)
sys.exit(0)