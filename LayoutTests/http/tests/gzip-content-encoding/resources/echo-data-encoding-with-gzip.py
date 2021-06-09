#!/usr/bin/env python3

import gzip
import os
import sys
import tempfile
from urllib.parse import parse_qs

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
q = query.get('q', [''])[0]
content_type = query.get('type', ['text/plain'])[0]

sys.stdout.write(
    'Content-Type: {}\r\n'
    'Content-Encoding: gzip\r\n'.format(content_type)
)

with gzip.open(os.path.join(os.path.dirname(__file__), 'content.gz'), 'wb') as file:
    file.write(bytes(q, 'utf-8'))

with open(os.path.join(os.path.dirname(__file__), 'content.gz'), 'rb') as file:
    content = file.read()

    sys.stdout.write('Content-Length: {}\r\n\r\n'.format(len(content)))
    sys.stdout.flush()
    sys.stdout.buffer.write(content)

if os.path.isfile(os.path.join(os.path.dirname(__file__), 'content.gz')):
    os.remove(os.path.join(os.path.dirname(__file__), 'content.gz'))
