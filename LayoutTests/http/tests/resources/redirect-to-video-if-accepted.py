#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

video = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('video', [''])[0]

sys.stdout.write('Content-Type: text/html\r\n')

if 'video/*' in os.environ.get('HTTP_ACCEPT', ''):
    sys.stdout.write(
        'status: 301\r\n'
        'Location: {}\r\n'
        'Cache-Control: no-cache, must-revalidate\r\n\r\n'.format(video)
    )
    sys.exit(0)

sys.stdout.write('status: 400\r\n\r\n')
