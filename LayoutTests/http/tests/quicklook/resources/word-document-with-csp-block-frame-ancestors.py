#!/usr/bin/env python3

import os
import sys

sys.stdout.write(
    'Content-Type: application/vnd.openxmlformats-officedocument.wordprocessingml.document\r\n'
    'Content-Security-Policy: frame-ancestors \'none\'\r\n\r\n'
)

sys.stdout.flush()
with open(os.path.join('/'.join(__file__.split('/')[0:-1]), 'pass.docx'), 'rb') as file:
    sys.stdout.buffer.write(file.read())