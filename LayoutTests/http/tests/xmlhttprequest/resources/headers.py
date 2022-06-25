#!/usr/bin/env python3
import os
import sys

sys.stdout.write(
    'Content-Type: text/plain\r\n'
    'X-Custom-Header: test\r\n'
    'Set-Cookie: test\r\n'
    'Set-Cookie2: test\r\n'
    'X-Custom-Header-Empty:\r\n'
    'X-Custom-Header-Comma: 1\r\n'
    'X-Custom-Header-Comma: 2\r\n'
    'X-Custom-Header-Bytes: …\r\n'
    '\r\n'
    'TEST\n'
)
