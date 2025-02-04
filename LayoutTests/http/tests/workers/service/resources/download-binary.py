#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Content-Type: application/my-super-binary\r\n\r\n'
    '[1, 2, 3]'
)
