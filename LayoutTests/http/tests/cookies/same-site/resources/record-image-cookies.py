#!/usr/bin/env python3
#
# Helper for img-from-srcdoc-iframe-inside-cross-origin-iframe.html
# (rdar://175498842 / https://bugs.webkit.org/show_bug.cgi?id=313220).
#
# Modes:
#   ?mode=reset&token=<id> ............ clear any previously recorded cookies for <id>
#   ?mode=record&token=<id> ........... record incoming Cookie header to a temp file keyed by <id>; respond with a 1x1 PNG
#   ?mode=read&token=<id> ............. respond with JSON of cookies recorded for <id>

import json
import os
import sys
import tempfile
from urllib.parse import parse_qs

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file)))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_cookies, get_state, set_state

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
mode = query.get('mode', ['record'])[0]
token = query.get('token', ['default'])[0]
tmp_file = os.path.join(tempfile.gettempdir(), 'srcdoc-img-cookies-' + token + '.json')

origin = os.environ.get('HTTP_ORIGIN', '*')

if mode == 'reset':
    set_state(tmp_file, '{}')
    sys.stdout.write(
        'Content-Type: text/plain\r\n'
        'Cache-Control: no-store\r\n'
        'Access-Control-Allow-Origin: {}\r\n'
        'Access-Control-Allow-Credentials: true\r\n\r\n'
        'reset'.format(origin)
    )
    sys.exit(0)

if mode == 'read':
    cookies_json = get_state(tmp_file, '{}')
    sys.stdout.write(
        'Content-Type: application/json\r\n'
        'Cache-Control: no-store\r\n'
        'Access-Control-Allow-Origin: {}\r\n'
        'Access-Control-Allow-Credentials: true\r\n\r\n'
        '{}'.format(origin, cookies_json)
    )
    sys.exit(0)

# mode == 'record' (default): record incoming cookies and serve a 1x1 transparent PNG.
cookies = get_cookies()
set_state(tmp_file, json.dumps(cookies))

png = bytes.fromhex(
    '89504e470d0a1a0a0000000d49484452000000010000000108060000001f15c489'
    '0000000d4944415478da63f8ffff3f0005fe02fe2c8c5d420000000049454e44ae'
    '426082'
)
sys.stdout.buffer.write(
    ('Content-Type: image/png\r\n'
     'Cache-Control: no-store\r\n'
     'Content-Length: {}\r\n\r\n'.format(len(png))).encode()
)
sys.stdout.buffer.write(png)
sys.exit(0)
