#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

step = int(parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('step', [0])[0])

sys.stdout.write('Content-Type: text/html\r\n')

if step == 1:
    sys.stdout.write(
        'status: 302\r\n'
        'Location: http://localhost:8000/in-app-browser-privacy/resources/redirect.py?step=2\r\n\r\n'
    )
elif step == 2:
    sys.stdout.write(
        'status: 302\r\n'
        'Location: http://localhost:8000/in-app-browser-privacy/resources/redirect.py?step=3\r\n\r\n'
    )
elif step == 3:
    sys.stdout.write(
        'status: 200\r\n\r\n'
        'FAILED: Should not have loaded\n'
        '<script> if (window.testRunner) testRunner.notifyDone();</script>\n'
    )