#!/usr/bin/env python3

import os
import sys

uri = os.environ.get('REQUEST_URI', '')

sys.stdout.write(
    'Content-Type: text/plain\r\n'
    'Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n'
    'Cache-Control: no-store, no-cache, must-revalidate\r\n'
    'Pragma: no-cache\r\n\r\n'
    '{}'.format(uri)
)