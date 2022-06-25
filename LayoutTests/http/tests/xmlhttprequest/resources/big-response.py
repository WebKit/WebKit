#!/usr/bin/env python3
import os
import sys

sys.stdout.write('Content-Type: text/html\r\n\r\n')

for i in range(128 * 1024):
    sys.stdout.write('Chunk {}'.format(i))
