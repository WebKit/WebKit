#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Content-Type: text/html; charset=BOCU-1\r\n\r\n'
    'žŠÓÓ“Šˆ'
)
