#!/usr/bin/env python3

import os
import sys
import tempfile
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
filename = query.get('filename', ['404.txt'])[0]
data = query.get('data', [''])[0]

tmp_file = os.path.join(tempfile.gettempdir(), filename)
sys.stdout.write('Content-Type: text/html\r\n\r\n')

with open(tmp_file, 'w') as open_file:
    open_file.write(data)

sys.stdout.write(tmp_file)
