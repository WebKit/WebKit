#!/usr/bin/env python3

import sys
import time

padding = ' ' * 5000

sys.stdout.write(
    'Connection: keep-alive\r\n'
    'Content-type: multipart/x-mixed-replace; boundary=boundary\r\n\r\n'
    '--boundary\r\n'
    'Content-Type: text/html\r\n\r\n'
    f'{padding}\n'
    '\n'
    '<script>\n'
    'if (window.testRunner)\n'
    '    testRunner.dumpAsText();\n'
    '</script>\n\n'
)

for i in range(0, 11):
    sys.stdout.write(
        '\r\n--boundary\r\n'
        'Content-Type: text/plain\r\n\r\n'
        'This text should only appear once {}'
        '{}'.format(i, padding)
    )
    sys.stdout.flush()
    time.sleep(0.1)

sys.stdout.write('\r\n\r\n\r\n--boundary--\r\n')