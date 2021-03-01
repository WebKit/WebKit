#!/usr/bin/env python3
import json
import os
import sys

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from urllib.parse import unquote, unquote_to_bytes

query = os.environ.get('QUERY_STRING', '')
split_query = {}
for pair in query.split('&'):
    split_query[unquote(pair.split('=')[0])] = '='.join(pair.split('=')[1:])

sys.stdout.write('Content-Type: text/html\r\n')

origin = split_query.get('origin', 'none')
if origin != 'none' and '%00' not in origin:
    sys.stdout.write('Access-Control-Allow-Origin: ')
    sys.stdout.flush()
    sys.stdout.buffer.write(unquote_to_bytes(origin))
    sys.stdout.write('\r\n')

headers = split_query.get('headers')
if headers:
    sys.stdout.write('Access-Control-Allow-Headers: ')
    sys.stdout.flush()
    sys.stdout.buffer.write(unquote_to_bytes(headers))
    sys.stdout.write('\r\n')

methods = split_query.get('methods')
if methods:
    sys.stdout.write('Access-Control-Allow-Methods: ')
    sys.stdout.flush()
    sys.stdout.buffer.write(unquote_to_bytes(methods))
    sys.stdout.write('\r\n')

sys.stdout.write('\r\n')

headers = {}
headers['get_value'] = unquote(split_query.get('get_value', ''))
for headername, headervalue in os.environ.items():
    if headername in ['CONTENT_TYPE', 'CONTENT_LENGTH']:
        headers[headername.lower().replace('_', '-')] = headervalue
    if not headername.startswith('HTTP_'):
        continue
    headers[headername[5:].lower().replace('_', '-')] = headervalue

sys.stdout.write(json.dumps(headers))
