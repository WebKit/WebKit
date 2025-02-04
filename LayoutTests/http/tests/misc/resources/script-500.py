#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'status: 500\r\n'
    'Content-Type: text/html\r\n\r\n'
    'document.getElementById(\'result\').innerHTML = \'FAIL\';\n'
)
