#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

path = os.path.abspath(parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('path', [__file__])[0])

sys.stdout.write('Content-Type: text/html\r\n\r\n')

if os.path.isfile(path):
    sys.stdout.write(str(os.stat(path).st_mtime).split('.')[0])
