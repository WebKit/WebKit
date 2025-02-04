#!/usr/bin/env python3
import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion
import time

time.sleep(10)

sys.stdout.write(
    'Content-Type: text/xml\r\n'
    '\r\n'
)
