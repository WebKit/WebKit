#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
credentials = query.get('credentials', [None])[0]
fail = query.get('fail', [None])[0]

sys.stdout.write(
    'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
    'Content-Type: application/javascript\r\n'
)

if credentials is not None:
    if credentials.lower() == 'true':
        sys.stdout.write('Access-Control-Allow-Credentials: true\r\n')
    else:
        sys.stdout.write('Access-Control-Allow-Credentials: false\r\n')

sys.stdout.write('\r\n')

if fail is not None and fail.lower() == 'true':
    sys.stdout.write('throw({toString: function(){ return \'SomeError\' }});')
else:
    sys.stdout.write('alert(\'script ran.\');')
