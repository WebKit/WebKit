#!/usr/bin/env python3

import sys
import time

now = time.time()
now = str(now).split('.')
now = '{} {}'.format(now[1], now[0])

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<h1>Page 3</h1>\n'
    '<span id="submissionTime">0.{}</span><br/>\n'
    '<a id="link" href="javascript:history.back();">go back</a>\n'.format(now)
)