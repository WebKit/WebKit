#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Location: https://127.0.0.1:8443/misc/resources/webtiming-ssl.html\r\n'
    'status: 302\r\n'
    'Content-Type: text/html\r\n\r\n'
)
