#!/usr/bin/env python3
import os
import sys
import tempfile

from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
stateFile = os.path.join(tempfile.gettempdir(), query.get('filename', ['state.txt'])[0])

sys.stdout.write('Content-Type: text/html\r\n')
sys.stdout.write('\r\n')

try:
    if os.path.exists(stateFile):
        os.remove(stateFile)
except OSError:
    print('FAIL: unable to write to file: {}'.format(stateFile))
