#!/usr/bin/env python3
import os
import sys
from urllib.parse import parse_qs

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Access-Control-Allow-Origin: *\r\n'
    'Access-Control-Allow-Methods: GET\r\n'
    'Access-Control-Allow-Headers: authorization, x-webkit, content-type\r\n'
)

if os.environ.get('REQUEST_METHOD') == 'OPTIONS':
    sys.stdout.write(
        'status: 200\r\n'
        '\r\n'
    )
    sys.exit(0)

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)

if query.get('redirect'):
    sys.stdout.write(
        'status: 302\r\n'
        'Location: {}\r\n'
        '\r\n'.format(query.get('url', [''])[0])
    )
    sys.exit(0)

sys.stdout.write(
    'status: 200\r\n'
    '\r\n'
)

for variable, description in [
    ('HTTP_X_WEBKIT', 'x-webkit'),
    ('HTTP_ACCEPT', 'accept'),
    ('CONTENT_TYPE', 'content-type'),
]:
    if os.environ.get(variable):
        sys.stdout.write('{} header found: {}\n'.format(description, os.environ.get(variable)))
    else:
        sys.stdout.write('not found any {} header found\n'.format(description))

if os.environ.get('HTTP_AUTHORIZATION'):
    sys.stdout.write('authorization header found')
else:
    sys.stdout.write('not found any authorization header')
