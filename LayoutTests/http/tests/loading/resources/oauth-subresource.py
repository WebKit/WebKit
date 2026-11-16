#!/usr/bin/env python3

import os
import sys

realm = os.environ.get('REQUEST_URI', '')

sys.stdout.write(
    'Cache-Control: no-store\r\n'
    'WWW-Authenticate: OAuth realm="{}"\r\n'
    'status: 401\r\n'
    'Content-Type: text/html\r\n\r\n'
    'Sent OAuth Challenge.'.format(realm)
)