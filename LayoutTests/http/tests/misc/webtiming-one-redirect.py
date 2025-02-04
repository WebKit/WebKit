#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Location: resources/webtiming-one-redirect.html\r\n'
    'status: 302\r\n'
    'Content-Type: text/html\r\n\r\n'
)
