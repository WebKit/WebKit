#!/usr/bin/env python3
import os
import sys


sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Access-Control-Allow-Origin: *\r\n'
    'Access-Control-Max-Age: 0\r\n'
)

if os.environ.get('REQUEST_METHOD') == 'OPTIONS':
    if 'x-custom-header' in os.environ.get('HTTP_ACCESS_CONTROL_REQUEST_HEADERS', '').split(','):
        sys.stdout.write('Access-Control-Allow-Headers: X-Custom-Header\r\n')
    sys.stdout.write('\r\n')

elif os.environ.get('REQUEST_METHOD') == 'GET':
    sys.stdout.write('\r\n')
    if os.environ.get('HTTP_X_CUSTOM_HEADER'):
        sys.stdout.write('PASS')
    else:
        sys.stdout.write('FAIL')
