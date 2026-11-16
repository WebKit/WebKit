#!/usr/bin/env python3

import random
import sys

sys.stdout.write(
    'Cache-Control: no-store\r\n'
    'Content-Type: text/html\r\n\r\n'
    'var p = document.createElement("p");\n'
    'p.appendChild(document.createTextNode("{}"));\n'
    'document.body.appendChild(p);\n'.format(random.randint(0, 32767))
)