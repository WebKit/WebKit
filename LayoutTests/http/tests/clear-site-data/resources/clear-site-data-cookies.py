#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Clear-Site-Data: "cookies"\r\n'
    'Content-Type: text/html\r\n\r\n'
)

print('FOO')
