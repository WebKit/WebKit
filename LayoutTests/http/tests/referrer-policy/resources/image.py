#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

referer = os.environ.get('HTTP_REFERER', '')
expected = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('expected', [''])[0]

if referer != expected:
    sys.stdout.write('Content-Type: text/plain\r\n\r\n{}'.format(referer))
    sys.exit(0)

sys.stdout.write('Content-type: image/png\r\n\r\n')

sys.stdout.flush()
with open(os.path.join(os.path.dirname(__file__), '../../security/contentSecurityPolicy/block-all-mixed-content/resources/red-square.png'), 'rb') as file:
    sys.stdout.buffer.write(file.read())
