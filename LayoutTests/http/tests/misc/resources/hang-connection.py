#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion
import time

sys.stdout.write('Content-Type: text/html\r\n\r\n')

while True:
    sys.stdout.write('a')
    sys.stdout.flush()
    time.sleep(0.1)
