#!/usr/bin/env python3
import os
import sys

sys.stdout.write(
    'Content-type: text/plain\r\n'
    '\r\n'
    '{}'.format(os.environ.get('HTTP_REFERER', 'NO REFERER'))
)
