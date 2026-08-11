#!/usr/bin/env python3

import os
import sys

if os.environ.get('HTTP_IF_MODIFIED_SINCE', ''):
    sys.stdout.write(
        'status: 304\r\n'
        'Cache-Control: private, s-maxage=0, max-age=0, must-revalidate\r\n'
        'Content-Type: text/html\r\n\r\n'
    )
else:
    sys.stdout.write(
        'Content-Type: text/html; charset=utf-8\r\n'
        'Cache-Control: private, s-maxage=0, max-age=0, must-revalidate\r\n'
        'Expires: Thu, 01 Jan 1970 00:00:00 GMT\r\n'
        'Vary: Accept-Encoding, Cookie\r\n'
        'Last-Modified: Mon, 1 Apr 2013 12:12:12 GMT\r\n\r\n'
        'You\'re on page 5.py\n'
    )