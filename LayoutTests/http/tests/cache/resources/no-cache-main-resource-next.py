#!/usr/bin/env python3

import sys
from random import randint

sys.stdout.write(
    'Cache-control: no-cache, max-age=0\r\n'
    'Last-Modified: Thu, 01 Jan 1970 00:00:00 GMT\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<script>\n'
    f'var randomNumber = {randint(0, sys.maxsize)};\n'
    'opener.postMessage(randomNumber, \'*\');\n'
    '</script>\n'
)