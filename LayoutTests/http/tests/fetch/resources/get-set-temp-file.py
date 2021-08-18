#!/usr/bin/env python3

import os
import sys
import tempfile
import time
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
cmd = query.get('cmd', [None])[0]
filename = query.get('filename', [None])[0]
data = query.get('data', [''])[0]
delay = int(query.get('delay', ['0'])[0])

time.sleep(delay / 1000.0)

tmp_file = os.path.join(tempfile.gettempdir(), os.path.basename(filename))

sys.stdout.write('Content-Type: text/plain\r\n\r\n')

if cmd == 'get':
    with open(tmp_file, 'r') as open_file:
        sys.stdout.write(open_file.read())
elif cmd == 'set':
    with open(tmp_file, 'w') as open_file:
        open_file.write(data)
    sys.stdout.write('Set {}\r\n'.format(tmp_file))
elif cmd == 'clear':
    if os.path.exists(tmp_file):
        os.remove(tmp_file)
        sys.stdout.write('Deleted {}\r\n'.format(tmp_file))
    else:
        sys.stdout.write('No such file: {}\r\n'.format(tmp_file))
else:
    sys.stdout.write('Unknown command\r\n')
