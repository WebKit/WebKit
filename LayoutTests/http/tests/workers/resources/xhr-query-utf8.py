#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('query', [''])[0]

sys.stdout.write(
    'Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n'
    'Cache-Control: no-cache, must-revalidate\r\n'
    'Pragma: no-cache\r\n'
    'Content-Type: text/html\r\n\r\n'
)

if query == '\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82'.encode('latin-1').decode('utf-8'):
    sys.stdout.write('PASS: XHR query was encoded in UTF-8: {}'.format(query))
else:
    sys.stdout.write('FAIL: XHR query was NOT encoded in UTF-8: {}'.format(query))