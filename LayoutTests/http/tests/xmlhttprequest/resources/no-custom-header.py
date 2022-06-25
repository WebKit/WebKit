#!/usr/bin/env python3
import os
import sys
import tempfile

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import set_state, get_state
from urllib.parse import parse_qs

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Access-Control-Allow-Origin: *\r\n'
    'Access-Control-Allow-Headers: X-Custom-Header\r\n'
    'Access-Control-Max-Age: 0\r\n'
    '\r\n'
)

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
stateFile = os.path.join(tempfile.gettempdir(), query.get('filename', ['state.txt'])[0])

if os.environ.get('REQUEST_METHOD') == 'OPTIONS':
    if os.environ.get('HTTP_X_CUSTOM_HEADER'):
        set_state(stateFile, 'FAIL')
    else:
        set_state(stateFile, 'PASS')
else:
    if os.environ.get('HTTP_X_CUSTOM_HEADER'):
        sys.stdout.write(get_state(stateFile, default='FAIL'))
    else:
        sys.stdout.write('FAIL - no header in actual request')
