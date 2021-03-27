#!/usr/bin/env python3

import os
import sys
import tempfile
from urllib.parse import parse_qs

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
filename = query.get('filename', ['state.txt'])[0]
data = query.get('data', [''])[0]

tmpFile = os.path.join(tempfile.gettempdir(), filename)
sys.stdout.write('Content-Type: text/html\r\n\r\n')

try:
    file = open(tmpFile, 'w')
    file.write(data)
    file.close()
except:
    sys.stdout.write(f'FAIL: unable to write to file: {tmpFile}\n')
    sys.exit(0)

sys.stdout.write(f'{tmpFile}\n')