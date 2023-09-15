#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
meta_value = query.get('meta_value', [''])[0]
http_value = query.get('http_value', [''])[0]

sys.stdout.write(
    'status: 200\r\n'
    'Referrer-Policy: {}\r\n'
    'Content-Type: text/html\r\n\r\n'
    '\r\n'
    '<!DOCTYPE html>\r\n'
    '<html>\r\n'
    '<head><meta name=\'referrer\' content=\'{}\'></head>\r\n'
    '<body>\r\n'
    '<script>\r\n'
    'onmessage = (msg) => top.postMessage(msg.data, "*");\r\n'
    'window.open("postReferrer.py", "innerPopup", "popup");\r\n'
    '</script>\r\n'
    '</body>\r\n'
    '</html>\r\n'.format(http_value, meta_value)
)
