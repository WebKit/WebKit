#!/usr/bin/env python3

import os
import sys

referer = os.environ.get('HTTP_REFERER', '')

sys.stdout.write(
    'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
    'Content-Type: text/html\r\n\r\n{}'.format(referer)
)