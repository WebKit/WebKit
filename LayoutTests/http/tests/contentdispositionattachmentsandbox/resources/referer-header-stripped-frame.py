#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

referrer = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('referrer', [''])[0]

sys.stdout.write(
    'Content-Disposition: attachment; filename=test.html\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<head>\n'
)

if referrer:
    sys.stdout.write('<meta name=\"referrer\" content=\"{}\">\n'.format(referrer))

sys.stdout.write(
    '<style>\n'
    'a {\n'
    '    display: block;\n'
    '    width: 100vw;\n'
    '    height: 100vh;\n'
    '}\n'
    '</style>\n'
    '</head>\n'
    '<a href="echo-http-referer.py">Link to echo-http-referer.py</a>\n'
)