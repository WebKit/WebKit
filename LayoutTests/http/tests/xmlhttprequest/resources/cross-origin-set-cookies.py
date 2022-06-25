#!/usr/bin/env python3
import os
import sys

from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Set-Cookie: WK-xhr-cookie-storage=MySpecialValue;{}\r\n'.format(
    'expires=Thu, 19 Mar 1982 11:22:11 GMT' if query.get('clear') is not None else '',
))

sys.stdout.write(
    'Cache-Control: no-store\r\n'
    'Last-Modified: Thu, 19 Mar 2009 11:22:11 GMT\r\n'
    'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
    'Access-Control-Allow-Credentials: true\r\n'
    '\r\n'
    'log(\'PASS: Loaded\')\n'
)
