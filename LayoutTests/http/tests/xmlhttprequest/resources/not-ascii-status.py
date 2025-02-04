#!/usr/bin/env python3
import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'status: 200 OK…\r\n'
    '\r\n'
    'OK…'
)
