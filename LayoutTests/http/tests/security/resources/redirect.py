#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
redirectURL = query.get('redirectTo', [''])[0]

for key in ['name2', 'name3', 'message']:
    value = query.get(key, [''])[0]
    if value:
        redirectURL = '{url}&{key}={value}'.format(url=redirectURL, key=key, value=value)

sys.stdout.write(
    'Location: {}\r\n'
    'Content-Type: text/html\r\n\r\n'.format(redirectURL)
)