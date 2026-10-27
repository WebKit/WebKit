#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

status = os.environ.get('HTTP_X_STATUS_CODE_NEEDED')
parameters = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)

if status == '304':
    sys.stdout.write(
        'Content-Type: text/html\r\n'
        'status: 304\r\n'
        '\r\n'
    )
else:
    mime_type = parameters.get('mimetype', ['text/plain'])[0]
    sys.stdout.write(
        f'Content-Type: {mime_type}\r\n'
        'Cache-Control: public, max-age=31556926\r\n'
    )
    if status == 'incomplete':
        sys.stdout.write('Content-Length: 100\r\n')
    sys.stdout.write('\r\n')

    content = parameters.get('content', ['Plain text resource.'])[0]
    content *= int(parameters.get('repeat', ['1'])[0])
    if status == 'incomplete':
        content = 'Partial response.'
    elif status == 'empty':
        content = ''
    sys.stdout.flush()
    sys.stdout.buffer.write(content.encode('utf-8'))
