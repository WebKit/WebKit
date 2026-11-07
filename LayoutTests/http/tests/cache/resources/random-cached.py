#!/usr/bin/env python3

import sys
from random import randint

sys.stdout.write(
    'Content-Type: text/javascript\r\n'
    'Cache-Control: max-age=60000\r\n\r\n'
    f'top.randomNumber = {randint(0, 999999999999)};\n'
    "top.document.querySelector('h1').textContent = top.randomNumber;"
)
