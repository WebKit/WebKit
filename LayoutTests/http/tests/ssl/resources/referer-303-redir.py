#!/usr/bin/env python3

import sys

sys.stdout.write(
    'status: 303\r\n'
    'Location: http://127.0.0.1:8000/ssl/resources/no-http-referer.cgi\r\n'
    'Cache-Control: no-cache,no-store\r\n'
    'Content-Type: text/html\r\n\r\n'
)