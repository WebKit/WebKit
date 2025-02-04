#!/usr/bin/env python3

import os
import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion
from urllib.parse import parse_qs

option = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('option', ['DENY'])[0]

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'X-Frame-Options: {}\r\n'
    '\r\n'.format(option)
)

sys.stdout.write(option)
