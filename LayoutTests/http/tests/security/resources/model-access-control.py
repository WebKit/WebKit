#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
allowAll = True if query.get('allowAll', [None])[0] is not None else False
origin = True if query.get('origin', [None])[0] is not None else False

if origin:
    sys.stdout.write('Access-Control-Allow-Origin: {}\r\n'.format(query.get('origin', [''])[0]))
elif allowAll:
    sys.stdout.write('Access-Control-Allow-Origin: *\r\n')
sys.stdout.write('Content-Type: model/vnd.usdz+zip\r\n\r\n')

sys.stdout.flush()
with open(os.path.join(os.path.dirname(__file__), query.get('file', [''])[0]), 'rb') as file:
    sys.stdout.buffer.write(file.read())
