#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'status: 301\r\n'
    'Location: data:application/javascript,success=true;\r\n'
    'Content-Type: text/html\r\n\r\n'
)
