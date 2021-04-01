#!/usr/bin/env python3

import sys

sys.stdout.write(
    'status: 307\r\n'
    'Location: http://localhost:8000/appcache/resources/simple.txt\r\n'
    'Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n'
    'Cache-Control: no-cache, must-revalidate\r\n'
    'Pragma: no-cache\r\n'
    'Content-Type: text/html\r\n\r\n'
)