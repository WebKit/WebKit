#!/usr/bin/env python3

import cgi
import os
import sys
from urllib.parse import parse_qs

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_cookies

request_method = os.environ.get('REQUEST_METHOD', '')
content_type = os.environ.get('CONTENT_TYPE', '')

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
redirected = query.get('redirected', [None])[0]

request = {}
if request_method == 'POST':
    form = cgi.FieldStorage()
    for key in form.keys():
        request.update({ key: form.getvalue(key) })
else:
    query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
    for key in query.keys():
        request.update({ key: query[key][0] })

request.update(get_cookies())

status = int(request.get('status', 200))

data = ''.join(sys.stdin.readlines())

sys.stdout.write('Content-Type: text/html\r\n')
if status > 200 and redirected is None:
    sys.stdout.write(
        'status: {}\r\n'
        'Location: redirect-methods-result.py?redirected=true\r\n\r\n'.format(status)
    )
    sys.exit(0)

sys.stdout.write(
    '\r\nRequest Method: {}<br> \n'
    'Request Body: {}<br>\n'
    'Request Content-Type: {}'.format(request_method, data, content_type)
)