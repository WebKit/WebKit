#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

to = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('to', [None])[0]

sys.stdout.write('Content-Type: text/html\r\n\r\n')

if to is not None:
    sys.stdout.write('<script>location.href=\'{}\';</script>'.format(to))

sys.stdout.write('<body>Redirecting</body>')