#!/usr/bin/env python3

import sys

sys.stdout.write(
    'status: 302\r\n'
    'Location: /cache/disk-cache/speculative-validation/resources/css-to-revalidate.py\r\n'
    'Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n'
    'ETag: foo\r\n'
    'Content-Type: text/html\r\n\r\n'
)