#!/usr/bin/env python3
import os
import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Content-Type: text/html\r\n'
    '\n'
    '{}\n'.format(os.environ.get('HTTP_USER_AGENT'))
)
