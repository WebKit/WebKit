#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Set-Cookie: one_cookie=shouldBeRejeced; domain=WrongDomain\r\n'
    'Location: cookies-wrong-domain-rejected-result.py\r\n'
    'Content-Type: text/html\r\n\r\n'
)
