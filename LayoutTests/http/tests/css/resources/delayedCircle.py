#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion
import time

time.sleep(0.01)

sys.stdout.write('Content-Type: image/svg+xml\r\n\r\n')

with open('./circle.svg', 'rb') as open_file:
    sys.stdout.flush()
    sys.stdout.buffer.write(open_file.read())
