#!/usr/bin/env python3

import sys
import time

sys.stdout.write(
    'Content-Type: text/plain\r\n\r\n'
    f'{int(time.time())}'
)
