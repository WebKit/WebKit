#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

charset = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('charset', [''])[0]

sys.stdout.write(
    'Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n'
    'Cache-Control: no-cache, must-revalidate\r\n'
    'Pragma: no-cache\r\n'
    'Content-Type: text/plain;\r\n\r\n'
)

if charset == 'koi8-r':
    sys.stdout.write('XHR: {}'.format('\xF0\xD2\xC9\xD7\xC5\xD4'.encode('latin-1').decode('koi8-r')))
else:
    sys.stdout.write('XHR: Привет')