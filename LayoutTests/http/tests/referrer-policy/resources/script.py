#!/usr/bin/env python3

import os
import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Cache: no-cache, no-store\r\n'
    'Content-Type: text/html\r\n\r\n'
    'checkReferrer(\'{}\');'.format(os.environ.get('HTTP_REFERER', ''))
)
