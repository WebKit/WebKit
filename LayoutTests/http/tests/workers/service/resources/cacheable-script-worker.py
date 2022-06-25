#!/usr/bin/env python3

import sys
from random import randint

max_age = 12 * 31 * 24 * 60 * 60

sys.stdout.write(
    'Cache-Control: public, max-age={}\r\n'
    'Content-Type: text/javascript\r\n\r\n'.format(max_age)
)

randomId = ''
charset = 'ABCDEFGHIKLMNOPQRSTUVWXYZ0123456789'
for _ in range(0, 16):
    randomId += charset[randint(0, len(charset) - 1)]

sys.stdout.write(
    'let randomId = "{}";\n'
    'self.addEventListener("message", function(e) {{\n'
    '   e.source.postMessage(randomId);\n'
    '}});\n'.format(randomId)
)