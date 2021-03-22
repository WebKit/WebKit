#!/usr/bin/env python3

import sys
from random import randint

sys.stdout.write('Content-Type: text/javascript\r\n\r\n')

randomId = ''
charset = 'ABCDEFGHIKLMNOPQRSTUVWXYZ0123456789'
for _ in range(0, 16):
    randomId += charset[randint(0, len(charset) - 1)]

sys.stdout.write('let randomId = "{}";'.format(randomId))