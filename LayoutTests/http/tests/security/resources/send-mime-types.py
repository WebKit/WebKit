#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

mime_type = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('mt', [''])[0]

sys.stdout.write(
    'Content-Type: {mt}\r\n\r\n'
    '<script>'
    'alert(\'FAIL: {mt}\');'
    '</script>'.format(mt=mime_type)
)