#!/usr/bin/env python3

import os
import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

referer = os.environ.get('HTTP_REFERER', '')

sys.stdout.write(
    'Cache-Control: no-store, private, max-age=0\r\n'
    'Content-Type: text/plain\r\n\r\n'
    '{}'.format(referer)
)
