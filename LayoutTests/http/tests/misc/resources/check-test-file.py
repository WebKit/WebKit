#!/usr/bin/env python3

import cgi
import sys

form = cgi.FieldStorage()
data = form.getvalue('data')

sys.stdout.write(
    'Content-Type: text/html; charset=UTF-8\r\n\r\n'
    '<html>\n'
    '<head>\n'
    '<script>\n'
    'onload = function() {\n'
    '    if (window.testRunner)\n'
    '        testRunner.notifyDone();\n'
    '}\n'
    '</script>\n'
    '</head>\n'
    '<body>\n'
    '<h1>This tests verifies the test file included in the multipart form data:</h1>\n'
    '<pre>\n'
)

if data:
    sys.stdout.write(
        'PASS: File is present\n'
        'PASS: File upload was successful\n'
    )

    if len(data) > 0:
        print('PASS: File size is greater than 0')
    else:
        print('FAIL: File size is 0')
else:
    sys.stdout.write(
        'FAIL: File is missing\n'
    )

sys.stdout.write(
    '</pre>\n'
    '</body>\n'
    '</html>\n'
)