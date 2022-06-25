#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

def contentType(path):
    if path.endswith('.html'):
        return 'text/html'
    if path.endswith('.manifest'):
        return 'text/cache-manifest'
    if path.endswith('.js'):
        return 'text/javascript'
    if path.endswith('.xml'):
        return 'application/xml'
    if path.endswith('.xhtml'):
        return 'application/xhtml+xml'
    if path.endswith('.svg'):
        return 'application/svg+xml'
    if path.endswith('.xsl'):
        return 'application/xslt+xml'
    if path.endswith('.gif'):
        return 'image/gif'
    if path.endswith('.jpg'):
        return 'image/jpeg'
    if path.endswith('.png'):
        return 'image/png'
    return 'text/plain'

referer = os.environ.get('HTTP_REFERER', '')

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
path = query.get('path', [''])[0]
expected_referer = query.get('expected-referer', [''])[0]

if expected_referer == referer and os.path.isfile(path):
    sys.stdout.write(
        'status: 200\r\n'
        'Cache-control: no-store\r\n'
        f'Content-Type: {contentType(path)}\r\n\r\n'
    )

    sys.stdout.flush()
    with open(os.path.join(os.path.dirname(__file__), path), 'rb') as file:
        sys.stdout.buffer.write(file.read())
else:
    sys.stdout.write(
        'status: 404\r\n'
        'Content-Type: text/html\r\n\r\n'
    )
