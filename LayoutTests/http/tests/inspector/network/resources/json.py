#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Content-Type: application/json\r\n\r\n'
    '{\"version\": 1}'
)
