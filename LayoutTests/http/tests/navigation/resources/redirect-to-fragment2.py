#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Location: redirect-preserves-fragment.py#bar\r\n'
    'Content-Type: text/html\r\n\r\n'
)
