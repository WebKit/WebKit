#!/usr/bin/env python3
import os
import sys

from urllib.parse import parse_qs
query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)

sys.stdout.write('Content-Type: text/html\r\n')

if os.environ.get('REQUEST_METHOD') == 'OPTIONS':
    if query.get('shouldPreflight') is None:
        sys.stdout.write(
            'status: 404\r\n'
            '\r\n'
        )
        sys.exit(0)
    if query.get('explicitlyAllowHeaders') is not None:
        sys.stdout.write(
            'Access-Control-Allow-Methods: GET, OPTIONS\r\n'
            'Access-Control-Allow-Headers: Accept, Accept-Language, Content-Language\r\n'
        )

sys.stdout.write(
    'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
    '\r\n'
    '{dict}\n'
    '{raw}\n'.format(
        dict=query,
        raw=os.environ.get('QUERY_STRING', ''),
    )
)
