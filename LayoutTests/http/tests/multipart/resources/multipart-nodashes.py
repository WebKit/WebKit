#!/usr/bin/env python3

# Generates a multipart/x-mixed-replace response but doesn't
# include the -- before the boundary.

import os
import sys

boundary = 'cutHere'
sys.stdout.write(
    'Content-Type: multipart/x-mixed-replace; boundary={boundary}\r\n\r\n'
    '{boundary}\r\n'
    'Content-Type: image/png\r\n\r\n'.format(boundary=boundary)
)

sys.stdout.flush()
with open(os.path.join('/'.join(__file__.split('/')[0:-1]), 'green-100x100.png'), 'rb') as file:
    sys.stdout.buffer.write(file.read())