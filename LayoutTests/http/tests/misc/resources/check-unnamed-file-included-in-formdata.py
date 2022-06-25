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
    '    if (window.testRunner)\n'
    '        testRunner.notifyDone();\n'
    '</script>\n'
    '</head>\n'
    '<body>\n'
    '<h1>This tests verifies that files without a name are included in the multipart form data:</h1>\n'
    '<pre>\n'
)

if data is not None:
    sys.stdout.write('Test passed.')
else:
    sys.stdout.write('Test failed.')

sys.stdout.write(
    '</pre>\n'
    '</body>\n'
    '</html>\n'
)