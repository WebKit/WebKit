#!/usr/bin/env python3

import sys

sys.stdout.write(
    'status: 302\r\n'
    'Location: http://127.0.0.1:8000/resources/network-simulator.py?test=appcache-main-resource-main-resource-redirect&path=/appcache/resources/main-resource-redirect-frame-2.html\r\n'
    'Content-Type: text/html\r\n\r\n'
)