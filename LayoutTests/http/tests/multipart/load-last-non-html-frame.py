#!/usr/bin/env python3

import sys

sys.stdout.buffer.write(
    'Content-type: multipart/x-mixed-replace;boundary=asdf\r\n\r\n'
    '--asdf\n'
    'Content-type: text/plain\n'
    '\n'
    'This test passes if the last multipart frame is displayed.\n'
    'FAIL\n'
    '\n'
    '{padding}\r\n'
    '--asdf\n'
    'Content-type: text/plain\n'
    '\n'
    'This test passes if the last multipart frame is displayed.\n'
    'PASS\n'
    '{padding}\r\n'
    '--asdf--\n'.format(padding=' ' * 5000).encode()
)
