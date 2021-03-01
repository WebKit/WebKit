#!/usr/bin/env python3

import cgi
import os
import sys

request = {}
form = cgi.FieldStorage()
for key in sorted(form.keys()):
    if not form[key].filename and key:
        request[key] = form[request]

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<html><body>\n'
)

if len(request) == 0:
    sys.stdout.write('There were no POSTed form values.')
else:
    sys.stdout.write('Form values are:<br>')

for key in sorted(request.keys()):
    sys.stdout.write(
        '{} : {}'
        '<br>'.format(key, request[key])
    )

sys.stdout.write(
    '<script>\n'
    'if (window.testRunner)\n'
    '   testRunner.notifyDone();\n'
    '</script>\n'
    '</body>\n'
    '</html>\n'
)