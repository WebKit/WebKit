#!/usr/bin/env python3

import sys
from random import randint

sys.stdout.write(
    'Content-Type: text/javascript\r\n'
    'Cache-Control: no-store\r\n\r\n'
    f'randomNumber = {randint(0, 999999999999)};\n'
    "document.querySelector('h1').textContent = randomNumber;"
)
