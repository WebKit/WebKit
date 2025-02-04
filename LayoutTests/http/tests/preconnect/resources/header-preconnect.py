#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Link: <http://localhost:8000>; rel=preconnect\r\n'
    'Content-Type: text/html\r\n\r\n'
)
