#!/usr/bin/env python3

import cgi
import os
import sys
from urllib.parse import parse_qs

headers = {}
for headername, headervalue in os.environ.items():
    if not headername.startswith('HTTP_'):
        continue

    header = list(headername[5:].lower().replace('_', '-'))
    header[0] = header[0].upper()
    for i in range(1, len(header)):
        if header[i-1] == '-':
            header[i] = header[i].upper()
    
    headers[''.join(header)] = headervalue

content_type = os.environ.get('CONTENT_TYPE')
if content_type:
    headers['Content-Type'] = content_type

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
method = os.environ.get('REQUEST_METHOD')

sys.stdout.write('Content-Type: text/html\r\n\r\n')

sys.stdout.write('{\n')
sys.stdout.write('    "uri": "{}",\n'.format(os.environ.get('REQUEST_URI')))
sys.stdout.write('    "method": "{}",\n'.format(method))

sys.stdout.write('    "headers": {\n')
for key in headers:
    sys.stdout.write('        "{}": "{}",\n'.format(key, headers[key]))
sys.stdout.write('    },\n')

sys.stdout.write('    "get": {\n')
if method == 'GET':
    for key in query:
        sys.stdout.write('        "{}": "{}",\n'.format(key, query[key][0]))
sys.stdout.write('    },\n')

sys.stdout.write('    "post": {\n')
if method == 'POST':
    form = cgi.FieldStorage()
    for key in sorted(form.keys()):
        if not form[key].filename and key:
            sys.stdout.write('        "{}": "{}",'.format(key, form[key].value))

sys.stdout.write('    },\n')

sys.stdout.write('}\n')