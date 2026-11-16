#!/usr/bin/env python3
import os
import sys
import gzip

text = b'Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.'
compressed = gzip.compress(text, compresslevel=6)

sys.stdout.write(
    'Content-Encoding: gzip\r\n'
    'Content-Type: application/octet-stream\r\n'
    'Content-Length: {}\r\n'
    '\r\n'.format(len(compressed))
)

sys.stdout.flush()
os.write(sys.stdout.fileno(), compressed)
