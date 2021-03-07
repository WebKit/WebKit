#!/usr/bin/env python3

import os
import sys

sys.stdout.write(
    'status: 404\r\n'
    'Content-Length: 38616\r\n'
    'Content-Type: image/jpeg\r\n'
    'Last-Modified: Thu, 01 Jun 2006 06:09:43 GMT\r\n\r\n'
)

sys.stdout.flush()

with open(os.path.join('/'.join(__file__.split('/')[0:-1]), 'compass.jpg'), 'rb') as file:
    sys.stdout.buffer.write(file.read())