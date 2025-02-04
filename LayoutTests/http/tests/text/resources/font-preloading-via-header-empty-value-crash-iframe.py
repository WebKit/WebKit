#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Link: <http://127.0.0.1:8000/resources/font.ttf>; as=""; type="font/ttf"\r\n'
    'Content-Type: text/html\r\n'
    '\r\n'
    'TEST\n'
)
