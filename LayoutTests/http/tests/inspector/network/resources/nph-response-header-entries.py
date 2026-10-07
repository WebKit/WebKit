#!/usr/bin/env python3

import os
import sys
import time
from urllib.parse import parse_qs

mode = parse_qs(os.environ.get('QUERY_STRING', '')).get('mode', ['final'])[0]
body = 'Response'
headers = [
    ('Content-Type', 'text/plain'),
    ('Connection', 'close'),
    ('Cache-Control', 'no-store'),
    ('Last-Modified', 'Wed, 16 Sep 2026 12:00:00 GMT'),
    ('X-Example', 'one,two'),
    ('x-example', 'three'),
    ('Set-Cookie', 'first=one; Expires=Wed, 21 Oct 2037 07:28:00 GMT'),
    ('Set-Cookie', 'second=two; Expires=Wed, 21 Oct 2037 07:28:00 GMT; Path=/; Secure'),
    ('WWW-Authenticate', 'Digest realm="one,two", nonce="three,four"'),
    ('WWW-Authenticate', 'Basic realm="five,six"'),
    ('Proxy-Authenticate', 'Digest realm="seven,eight"'),
    ('Proxy-Authenticate', 'Basic realm="nine,ten"'),
]
status = '200 OK'
if mode == 'redirect':
    status = '302 Found'
    headers += [
        ('Location', 'nph-response-header-entries.py?mode=final'),
        ('Cache-Control', 'private'),
    ]
if mode == 'stream':
    body = 'x' * 2048
    headers.append(('Content-Length', str(len(body) * 200)))
else:
    headers.append(('Content-Length', str(len(body))))

sys.stdout.write('HTTP/1.1 {}\r\n'.format(status) + ''.join('{}: {}\r\n'.format(name, value) for name, value in headers) + '\r\n')
if mode == 'stream':
    try:
        for _ in range(200):
            sys.stdout.write(body)
            sys.stdout.flush()
            time.sleep(0.1)
    except BrokenPipeError:
        # The test cancels the request after inspecting its response headers.
        sys.stdout = open(os.devnull, 'w')
else:
    sys.stdout.write(body)
