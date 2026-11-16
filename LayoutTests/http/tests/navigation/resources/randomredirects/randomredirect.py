#!/usr/bin/env python3

import random
import sys

sys.stdout.write(
    'status: 302\r\n'
    'Content-Type: text/html; charset=utf-8\r\n'
    'Cache-Control: private, s-maxage=0, max-age=0, must-revalidate\r\n'
    'Expires: Thu, 01 Jan 1970 00:00:00 GMT\r\n'
    'Vary: Accept-Encoding, Cookie\r\n'
    'Connection: keep-alive\r\n'
    'Location: {}.py\r\n\r\n'.format(random.randint(0, 9))
)