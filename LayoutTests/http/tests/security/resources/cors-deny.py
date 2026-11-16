#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
creds = query.get('credentials', [''])[0]
fail = query.get('fail', [''])[0]

sys.stdout.write(
    'Access-Control-Allow-Origin: http://example.org/\r\n'
    'Content-Type: application/javascript\r\n'
)

if creds.lower() == 'true':
    sys.stdout.write('Access-Control-Allow-Credentials: true\r\n\r\n')
else:
    sys.stdout.write('Access-Control-Allow-Credentials: true\r\n\r\n')

if fail.lower() == 'true':
    sys.stdout.write('throw({toString: function(){ return \'SomeError\' }});')
else:
    sys.stdout.write('alert(\'script ran.\');')