#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

request_method = os.environ.get('REQUEST_METHOD', '')
charset = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('_charset_', [''])[0]

data = ''
if request_method == 'POST':
    data = ''.join(sys.stdin.readlines())

sys.stdout.write(
    'Content-Type: text/html; charset=UTF-8\r\n\r\n'
    '<html>\n'
    '<head>\n'
    '<title>Test for bug 19079</title>\n'
    '</head>\n'
    '<body>\n'
    '<p>\n'
    'This is a test for https://bugs.webkit.org/show_bug.cgi?id=19079, it send the submissions\n'
    'character encoding in hidden _charset_ field.\n'
    '</p>\n'
)

if request_method == 'POST':
    if data:
        sys.stdout.write('<p>PASSED: {} </p>'.format(data))
    else:
        sys.stdout.write('<p>PASSED: No _charset_ field </p>')

if request_method == 'GET':
    sys.stdout.write('<p>PASSED: {} </p>'.format(charset))

sys.stdout.write(
    '<script>\n'
    'if(window.testRunner)\n'
    '    testRunner.notifyDone();\n'
    '</script>\n'
    '\n'
    '</body>\n'
    '</html>\n'
)