#!/usr/bin/env python3

import os
import sys

if os.environ.get('HTTP_IF_MODIFIED_SINCE', None) is not None or os.environ.get('HTTP_IF_NONE_MATCH', None) is not None:
    sys.stdout.write(
        'status: 304\r\n'
        'Content-Type: text/html\r\n\r\n'
    )
    sys.exit(0)

sys.stdout.write(
    'Access-Control-Allow-Origin: *\r\n'
    'ETag: foo\r\n'
    'Last-Modified: Thu, 01 Jan 2000 00:00:00 GMT\r\n'
    'Cache-Control: max-age=0\r\n'
    'Content-Type: video/mp4\r\n'
    'Content-Length: {}\r\n\r\n'.format(os.path.getsize(os.path.join(os.path.dirname(__file__), '../../media/resources/reference.mov')))
)

sys.stdout.flush()
with open(os.path.join(os.path.dirname(__file__), '../../media/resources/reference.mov'), 'rb') as file:
    sys.stdout.buffer.write(file.read())
