#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

sys.stdout.write(
    'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
    'Access-Control-Allow-Headers: X-WebKit\r\n'
    'Content-Type: text/html\r\n\r\n'
)
