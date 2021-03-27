#!/usr/bin/env python3

import os
import sys

sys.stdout.write(
    'Access-Control-Allow-Origin: *\r\n'
    'Content-Type: text/css\r\n\r\n'
)

sys.stdout.flush()
with open(os.path.join('/'.join(__file__.split('/')[0:-1]), 'xorigincss1.css'), 'rb') as file:
    sys.stdout.buffer.write(file.read())