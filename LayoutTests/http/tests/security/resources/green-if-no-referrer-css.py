#!/usr/bin/env python3

import os
import sys

referer = os.environ.get('HTTP_REFERER', '')

sys.stdout.write('Content-Type: text/css\r\n\r\n')

if not referer:
    sys.stdout.write('body { background-color: green; }')
else:
    sys.stdout.write('body { background-color: red; }')