#!/usr/bin/env python3

import os
import sys
from datetime import datetime, timedelta

modified_since = os.environ.get('HTTP_IF_MODIFIED_SINCE', '')

sys.stdout.write('Content-Type: text/html\r\n')

if modified_since:
    sys.stdout.write('status: 304\r\n\r\n')
    sys.exit(0)

one_year = 12 * 31 * 24 * 60 * 60
last_modified = '{} +0000'.format((datetime.utcnow() - timedelta(seconds=one_year)).strftime('%a, %d %b %Y %H:%M:%S'))
expires = '{} +0000'.format((datetime.utcnow() + timedelta(seconds=one_year)).strftime('%a, %d %b %Y %H:%M:%S'))

sys.stdout.write(
    f'Cache-Control: no-cache, max-age={one_year}\r\n'
    f'Expires: {expires}\r\n'
    'Content-Type: text/html\r\n'
    'Content-Length: 0\r\n'
    'Etag: 123456789\r\n'
    f'Last-Modified: {last_modified}\r\n\r\n'
)
sys.exit(0)