#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Location: about:blank\r\n'
    'status: 302\r\n'
    'Content-Type: text/html\r\n\r\n'
)
