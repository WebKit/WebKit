#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Content-Type: text/css, 200904131203\r\n\r\n'
    'html { background-color: green; }\n'
)
