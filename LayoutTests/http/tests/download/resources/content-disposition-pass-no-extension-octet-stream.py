#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Content-Disposition: Attachment; filename=PASS\r\n'
    'Content-Type: application/octet-stream\r\n\r\n'
    'Test file content.\n'
)
