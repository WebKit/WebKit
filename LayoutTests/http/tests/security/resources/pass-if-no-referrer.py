#!/usr/bin/env python3

import os
import sys

referer = os.environ.get('HTTP_REFERER', '')

if referer:
    sys.stdout.write('status: 500\r\n')
else:
    sys.stdout.write('status: 200\r\n')

sys.stdout.write('Content-Type: text/html\r\n\r\n')