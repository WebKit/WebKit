#!/usr/bin/env python3

import sys

sys.stdout.write(
    'status: 301\r\n'
    'Location: data:application/javascript,success=true;\r\n'
    'Content-Type: text/html\r\n\r\n'
)