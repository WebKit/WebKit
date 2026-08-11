#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

contentType = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('contentType', [''])[0]
sys.stdout.write(
    'Content-Type: {}\r\n\r\n\n'
    'id: 77\n'
    'retry: 300\n'
    'data: hello\n\n\n'.format(contentType)
)