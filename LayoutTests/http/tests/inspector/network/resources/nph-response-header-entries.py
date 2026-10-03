#!/usr/bin/env python3

import sys

body = 'Response'
headers = [
    ('Content-Type', 'text/plain'),
    ('Content-Length', str(len(body))),
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
sys.stdout.write('HTTP/1.1 200 OK\r\n' + ''.join('{}: {}\r\n'.format(name, value) for name, value in headers) + '\r\n' + body)
