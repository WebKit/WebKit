#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Disposition: attachment; filename=test file.txt\r\n'
    'Content-Type: text/plain\r\n\r\n'
    'Test file content.\n'
)