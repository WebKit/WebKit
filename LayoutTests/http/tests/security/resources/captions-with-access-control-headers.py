#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
origin = query.get('origin', [''])[0]
creds = query.get('credentials', [''])[0]

if origin:
    sys.stdout.write('Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n')
if creds:
    sys.stdout.write('Access-Control-Allow-Credentials: true\r\n')
sys.stdout.write('Content-Type: text/vtt\r\n\r\n')

sys.stdout.flush()
with open(os.path.join(os.path.dirname(__file__), 'captions.vtt'), 'rb') as file:
    sys.stdout.buffer.write(file.read())
