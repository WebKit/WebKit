#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy: sandbox\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<body>\n'
    '<!-- This test passes if it doesn\'t alert FAIL. -->\n'
    '<iframe src="data:text/html,<script>alert(\'FAIL\');</script>" width="100" height="100"></iframe>\n'
    '</body>\n'
    '</html>\n'
)