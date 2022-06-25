#!/usr/bin/env python3
import os
import sys

method = os.environ.get('REQUEST_METHOD', 'GET')

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'REQMETHOD: {}\r\n'
    '\r\n'.format(method)
)

if method != 'HEAD':
    print(method)
