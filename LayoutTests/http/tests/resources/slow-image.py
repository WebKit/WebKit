#!/usr/bin/env python3

import os
import sys
import time

time.sleep(0.4)

sys.stdout.write('Content-Type: image/png\r\n\r\n')
with open('square100.png', 'rb') as open_file:
    sys.stdout.flush()
    sys.stdout.buffer.write(open_file.read())
