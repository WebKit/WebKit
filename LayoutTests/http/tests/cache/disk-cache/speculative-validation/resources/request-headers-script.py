#!/usr/bin/env python3

import os
import sys

headers = {}
for headername, headervalue in os.environ.items():
    if not headername.startswith('HTTP_'):
        continue
    headers['-'.join(' '.join(headername[5:].lower().replace('_', '-').split('-')).title().split(' '))] = headervalue

sys.stdout.write(
    'Content-Type: text/javascript\r\n'
    'Cache-Control: max-age=0\r\n'
    'Etag: 123456789\r\n\r\n'
    'allRequestHeaders = [];\n'
)

for key in headers:
    sys.stdout.write(f'allRequestHeaders[\'{key}\'] = \'{headers[key]}\';')