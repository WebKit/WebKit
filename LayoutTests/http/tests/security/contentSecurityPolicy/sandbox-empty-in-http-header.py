#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy: sandbox\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<body>\n'
    '<!-- This test passes if it doesn\'t alert FAIL. -->\n'
    '<script>alert(\'FAIL\')</script>\n'
    '</body>\n'
    '</html>\n'
)