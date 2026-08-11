#!/usr/bin/env python3
import os
import sys

sys.stdout.write(
    'Content-Type: text/plain\r\n'
    '\r\n'
    '{}\n'.format(os.environ.get('HTTP_HOST'))
)
