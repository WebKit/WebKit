#!/usr/bin/env python3

import sys
import time

sys.stdout.write('Content-Type: text/html\r\n\r\n')

while True:
    sys.stdout.write('a')
    sys.stdout.flush()
    time.sleep(0.1)