#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Location: http://localhost/contentextensions/resources/should-not-load.html\r\n'
    'status: 302\r\n'
    'Content-Type: text/html\r\n\r\n'
)