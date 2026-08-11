#!/usr/bin/env python3

import os
import sys
import tempfile
from urllib.parse import parse_qs

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_state, set_state

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
filename = query.get('filename', ['404.txt'])[0]
mode = query.get('mode', [''])[0]

tmp_file = os.path.join(tempfile.gettempdir(), filename)
current_count = int(get_state(tmp_file, 0))

if mode == 'getFont':
    set_state(tmp_file, str(current_count + 1))
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
        '{}'.format(current_count)
    )