#!/usr/bin/env python3
import os
import sys
import time

sys.stdout.write('Content-Type: text/html\r\n')

if os.environ.get('REQUEST_METHOD') == 'GET':
    sys.stdout.write(
        'status: 302\r\n'
        'Location: http://localhost:8000/xmlhttprequest/resources/redirect-cors-origin-null-pass.py\r\n'
    )

if os.environ.get('HTTP_ORIGIN'):
    sys.stdout.write('Access-Control-Allow-Origin: {}\r\n'.format(
        os.environ.get('HTTP_ORIGIN')
    ))

sys.stdout.write('\r\n')
