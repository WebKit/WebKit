#!/usr/bin/env python3

import sys
import time

# Holds the response long enough for a navigation to this resource to stay provisional.
time.sleep(5)

sys.stdout.write('Content-Type: text/html\r\n\r\n')
sys.stdout.write('<body>loaded</body>')
