#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Connection: close\r\n'
    'Content-Type: text/html\r\n\r\n'
    'This should close a TCP connection in the client.\n'
)
