#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Disposition: attachment; filename=test.html\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<link rel="stylesheet" href="data:text/css,body::after { content: \'FAIL\'; }">\n'
)