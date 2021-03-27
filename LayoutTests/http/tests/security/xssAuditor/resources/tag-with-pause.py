#!/usr/bin/env python3

import sys
import time

sys.stdout.write(
    'Content-Type: text/html; charset=utf-8\r\n\r\n'
    '{}<body>\n'
    '<a ona{}\n'
    '{}'.format('A'*2048, 'a'*2000)
)

sys.stdout.flush()
time.sleep(0.2)

sys.stdout.write(
    'click=alert(1) ttt>\n'
    'Done.\n'
)