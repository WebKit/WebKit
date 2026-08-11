#!/usr/bin/env python3

import os
import sys

max_age = 12 * 31 * 24 * 60 * 60

sys.stdout.write(
    f'Cache-Control: public, max-age={max_age}\r\n'
    'Content-Type: text/javascript\r\n\r\n'
)

sys.stdout.write(
    'fetch("foo.txt").then((response) => {\n'
    '    response.text().then((responseText) => {\n'
    '        postMessage(responseText);\n'
    '    });\n'
    '});\n'
)
