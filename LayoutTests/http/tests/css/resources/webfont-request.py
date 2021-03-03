#!/usr/bin/env python3

import os
import sys
import tempfile
from urllib.parse import parse_qs

def getRequestCount(file):
    if not os.path.isfile(file):
        return 0

    with open(file, 'r') as file:
        return int(file.read())

def setRequestCount(file, count):
    with open(file, 'r') as file:
        file.write(count)

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
filename = query.get('filename', [''])[0]
mode = query.get('mode', [''])[0]

tmpFile = os.path.join(tempfile.gettempdir(), filename)
currentCount = getRequestCount(tmpFile)

if mode == 'getFont':
    setRequestCount(tmpFile, currentCount + 1)
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
        '{}'.format(currentCount)
    )