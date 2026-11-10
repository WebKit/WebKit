#!/usr/bin/env python3

import os
import re
import sys
import tempfile
import time
from urllib.parse import parse_qs

# Holds the response until a request to release-load.py with the same token creates
# the flag file, then serves `name`. Unlike a fixed load-and-stall.py stall, the test
# controls when the load completes, so "parsed but still loading" does not depend on
# how long the surrounding machinery took. The 30 second backstop matches the test
# cases' own timeout: a wedged run does not keep an Apache worker much past the test,
# and a healthy run always releases first.

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
token = query.get('token', [''])[0]
name = query.get('name', [''])[0]
mime_type = query.get('mimeType', [''])[0]

if not re.match(r'^[A-Za-z0-9._-]{1,64}$', token) or not os.path.isfile(name):
    sys.stdout.write('Content-Type: text/html\r\n\r\n')
    sys.exit(0)

flag = os.path.join(tempfile.gettempdir(), 'WebKit-load-when-released-' + token)
deadline = time.time() + 30
while not os.path.exists(flag) and time.time() < deadline:
    time.sleep(0.05)

try:
    os.unlink(flag)
except OSError:
    pass

name = os.path.abspath(name)
with open(name, 'rb') as open_file:
    content = open_file.read()

sys.stdout.write(
    'Content-Type: {}\r\n'
    'Content-Length: {}\r\n\r\n'.format(mime_type, len(content))
)
sys.stdout.flush()
sys.stdout.buffer.write(content)
sys.stdout.flush()
