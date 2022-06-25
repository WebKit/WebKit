#!/usr/bin/env python3

import os
import sys

content_type = os.environ.get('CONTENT_TYPE', '')
data = ''.join(sys.stdin.readlines())

sys.stdout.write(
    'Content-Type: text/html; charset=UTF-8\r\n\r\n'
    '<html>\n'
    '<body>\n'
    '<p>\n'
    'This test makes sure that forms POSTed with a content-type of text/plain actually send data in text/plain\n'
    '</p>\n'
)

if content_type == 'text/plain':
    sys.stdout.write('<p>SUCCESS: Content-type is text/plain.</p>')
else:
    sys.stdout.write('<p>FAIL: Content-type should be text/plain, but was \'{}\'</p>'.format(content_type))

if data == 'f1=This is field #1 &!@$%\r\n=\'<>\r\nf2=This is field #2 ""\r\n':
    sys.stdout.write('<p>SUCCESS</p>')
else:
    sys.stdout.write('<p>FAILURE: {}</p>'.format(data))

sys.stdout.write(
    '<script>\n'
    'if(window.testRunner)\n'
    '    testRunner.notifyDone();\n'
    '</script>\n'
    '</body>\n'
    '</html>\n'
)