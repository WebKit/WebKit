#!/usr/bin/env python3

import sys
import time

sys.stdout.write('Content-Type: text/plain\r\n')
sys.stdout.write('Cache-Control: no-store\r\n')
sys.stdout.write('\r\n')
sys.stdout.flush()

time.sleep(10)

sys.stdout.write('slow response body\n')
