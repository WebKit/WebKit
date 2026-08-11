#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

q = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('q', [''])[0]

sys.stdout.write(
    'Content-Type: text/html; charset=UTF-8\r\n\r\n'
    '<html><body><div id=\'output\'>'
    '{}'
    '</div></body></html>'.format(q)
)