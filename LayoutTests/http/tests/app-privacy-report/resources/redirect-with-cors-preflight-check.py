#!/usr/bin/env python3

import os
import sys

if_none_match = os.environ.get('HTTP_IF_NONE_MATCH', None)
if_modified_since = os.environ.get('HTTP_IF_MODIFIED_SINCE', None)

sys.stdout.write('Content-Type: text/html\r\n')

if if_none_match is not None or if_modified_since is not None:
    sys.stdout.write('status: 500\r\n\r\n')
    sys.exit(0)

request_method = os.environ.get('REQUEST_METHOD', '')
ac_request_method = os.environ.get('HTTP_ACCESS_CONTROL_REQUEST_METHOD', '')


sys.stdout.write(
    'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
    'Access-Control-Allow-Headers: X-WebKit\r\n'
)

if request_method == 'OPTIONS' and ac_request_method == 'GET':
    sys.stdout.write('\r\n')
    sys.exit(0)

header_string_value = os.environ.get('HTTP_X_WEBKIT', '')
sys.stdout.write(
    'status: 301\r\n'
    'ETag: "WebKitTest"\r\n'
    'Location: http://localhost:8000/app-privacy-report/resources/cors-preflight.py?value={}\r\n\r\n'.format(header_string_value)
)
