#!/usr/bin/env python3

import os
import re
import sys

http_accept = os.environ.get('HTTP_ACCEPT', '')

sys.stdout.write('Content-Type: text/html\r\n')

if re.findall('/*/*/', http_accept):
    sys.stdout.write(
        '\r\n'
        '        p#target { position: relative; }\n'
        '        /* This stylesheet is served as text/html */\n'
    )
else:
    sys.stdout.write('status: 406\r\n\r\n')