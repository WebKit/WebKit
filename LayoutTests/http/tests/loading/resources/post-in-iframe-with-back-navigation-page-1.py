#!/usr/bin/env python3

import sys
import time

now = time.time()
now = str(now).split('.')
now = '{} {}'.format(now[1], now[0])

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<h1>Page 1</h1>\n'
    '<span id="submissionTime">0.{}</span><br/>\n'
    '<form action="./post-in-iframe-with-back-navigation-page-2.py" name="form" method="POST">\n'
    '</form>\n'
    '<a id="link" href="javascript:document.form.submit();">to page 2</a>\n'.format(now)
)