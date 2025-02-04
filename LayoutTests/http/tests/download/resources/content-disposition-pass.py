#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Content-Disposition: Attachment; filename=PASS.txt\r\n'
    'Content-Type: text/plain\r\n\r\n'
    'Test file content.\n'
)
