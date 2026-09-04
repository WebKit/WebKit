#!/usr/bin/env python3

import os
import sys

font = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..', 'resources', 'Ahem.ttf')

sys.stdout.write(
    'Cache-Control: no-store\r\n'
    'Content-Type: font/ttf\r\n'
    'Access-Control-Allow-Origin: *\r\n\r\n'
)
sys.stdout.flush()

with open(font, 'rb') as file:
    sys.stdout.buffer.write(file.read())
