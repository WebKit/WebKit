#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Content-Type: text/css; charset=utf-8\r\n\r\n'
    'html { background-color: green; }\n'
)
