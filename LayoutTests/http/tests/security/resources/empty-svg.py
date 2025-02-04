#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Content-Type: image/svg+xml\r\n\r\n'
    '<svg xmlns="http://www.w3.org/2000/svg"></svg>\n'
)
