#!/usr/bin/env python3

import os
import sys

refer = os.environ.get('HTTP_REFERER', '')

sys.stdout.write(
    'Cache: no-cache, no-store\r\n'
    'Content-Type: text/html\r\n\r\n'
)

if 'login' in refer.lower():
    print('log(\'External script: FAIL\')')
else:
    print('log(\'External script: PASS\')')