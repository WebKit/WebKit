#!/usr/bin/env python3

import os
import sys
import tempfile
from urllib.parse import parse_qs

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

test = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('test', [None])[0]

if test is not None:
    ping_filepath = os.path.join(tempfile.gettempdir(), '{}.ping.txt'.format(test.replace('/', '-')))
else:
    sys.stdout.write('Content-Type: text/html\r\n\r\n')
    sys.exit(0)