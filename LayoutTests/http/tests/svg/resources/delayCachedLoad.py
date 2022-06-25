#!/usr/bin/env python3

import sys
import time

# Delay load by 0.07s. This was found to be the lowest value
# required for webkit.org/b/106733
time.sleep(0.07)

sys.stdout.write(
    'Cache-Control: no-cache, must-revalidate\r\n'
    'Location: http://127.0.0.1:8000/svg/resources/greenSquare.svg\r\n'
    'Content-Type: text/html\r\n\r\n'
)