#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)

referer = os.environ.get('HTTP_REFERER', '')
id = query.get('id', [''])[0]

if len(id) > 0:
    payload = "{{ id:{}, referer:'{}' }}".format(id, referer)
else:
    payload = "'{}'".format(referer)

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<body>\n'
    '<script>\n'
    'window.opener.postMessage({}, "*");\n'
    '</script>\n'
    '</body>\n'.format(payload)
)
