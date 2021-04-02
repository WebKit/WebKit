#!/usr/bin/env python3

import os
import sys
import tempfile
from urllib.parse import parse_qs

def get_request_count(file):
    if not os.path.isfile(file):
        return 0

    with open(file, 'r') as file:
        return int(file.read())

def set_request_count(file, count):
    with open(file, 'r') as file:
        file.write(count)

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
filename = query.get('filename', [''])[0]
mode = query.get('mode', [''])[0]

tmp_file = os.path.join(tempfile.gettempdir(), filename)
current_count = get_request_count(tmp_file)

if mode == 'getFont':
    set_request_count(tmp_file, current_count + 1)
    sys.stdout.write(
        'Access-control-max-age: 0\r\n'
        'Access-control-allow-origin: *\r\n'
        'Access-control-allow-methods: *\r\n'
        'Cache-Control: max-age=0\r\n'
        'Content-Type: application/octet-stream\r\n\r\n'
    )
else:
    sys.stdout.write(
        'Access-control-max-age: 0\r\n'
        'Access-control-allow-origin: *\r\n'
        'Access-control-allow-methods: *\r\n\r\n'
        '{}'.format(current_count)
    )