#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Location: http://localhost:xyz/none-existed.html\r\n'
    'Content-Type: text/html\r\n\r\n'
)
