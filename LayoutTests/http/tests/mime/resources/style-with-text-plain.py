#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Content-Type: text/plain\r\n\r\n'
    'html { background-color: red; }\n'
)
