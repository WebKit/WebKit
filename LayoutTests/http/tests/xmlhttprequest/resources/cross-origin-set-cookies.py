#!/usr/bin/env python3
import os
import sys

from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)

sys.stdout.buffer.write(
    'Content-Type: text/html\r\n'
    'Set-Cookie: WK-xhr-cookie-storage=MySpecialValue;{}\r\n'.format(
    'expires=Thu, 19 Mar 1982 11:22:11 GMT' if query.get('clear') is not None else '',
    ).encode())

sys.stdout.buffer.write(
    b'Cache-Control: no-store\r\n'
    b'Last-Modified: Thu, 19 Mar 2009 11:22:11 GMT\r\n'
    b'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
    b'Access-Control-Allow-Credentials: true\r\n'
    b'\r\n'
    b'log(\'PASS: Loaded\')\n'
)
