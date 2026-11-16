#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Disposition: attachment; filename=test.html\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<meta http-equiv="refresh" content="0; url=data:text/html,FAIL">\n'
)