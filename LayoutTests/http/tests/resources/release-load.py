#!/usr/bin/env python3

import os
import re
import sys
import tempfile
from urllib.parse import parse_qs

# Releases a response held by load-when-released.py: the holder serves its file once
# the flag for its token exists. Any origin can release any token; the token is what
# ties the two requests together, and being unique per run it doubles as a cache buster.

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
token = query.get('token', [''])[0]

if re.match(r'^[A-Za-z0-9._-]{1,64}$', token):
    flag = os.path.join(tempfile.gettempdir(), 'WebKit-load-when-released-' + token)
    open(flag, 'w').close()

sys.stdout.write('Content-Type: text/plain\r\n\r\nreleased')
