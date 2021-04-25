#!/usr/bin/env python3

import os
import sys
from datetime import datetime, timedelta

modified_since = os.environ.get('HTTP_IF_MODIFIED_SINCE', '')

sys.stdout.write('Content-Type: text/html\r\n')

if modified_since:
    sys.stdout.write('status: 304\r\n\r\n')
    sys.exit(0)

one_year = 365 * 24 * 60 * 60
last_modified = '{} +0000'.format((datetime.utcnow() - timedelta(seconds=one_year)).strftime('%a, %d %b %Y %H:%M:%S'))
expires = '{} +0000'.format((datetime.utcnow() + timedelta(seconds=one_year)).strftime('%a, %d %b %Y %H:%M:%S'))

sys.stdout.write(
    'Cache-Control: no-cache, max-age={}\r\n'
    'Expires: {}\r\n'
    'Content-Type: text/html\r\n'
    'Etag: 123456789\r\n'
    'Last-Modified: {}\r\n'
    'X-FRAME-OPTIONS: ALLOWALL\r\n\r\n'
    '<body><script>\n'
    'window.onload = function() {{ window.parent.test(); }}\n'
    '</script></body>\n'.format(one_year, expires, last_modified)
)