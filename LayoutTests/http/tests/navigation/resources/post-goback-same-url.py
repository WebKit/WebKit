#!/usr/bin/env python3

import cgi
import os
import sys

request_method = os.environ.get('REQUEST_METHOD', '')

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<html>\n'
    '<head>\n'
    '<script>\n'
    '\n'
    'function submitForm()\n'
    '{\n'
    '    document.getElementById(\'submit\').click();\n'
    '}\n'
    '\n'
    '</script>\n'
    '</head>\n'
    '<body>\n'
)

request = {}
if request_method == 'POST':
    form = cgi.FieldStorage()
    for key in form.keys():
        request[key] = form.getvalue(key)

if request.get('textdata', '') == 'foo':
    sys.stdout.write('You should not be seeing this text!')

sys.stdout.write(
    '<form method="POST">\n'
    '    <input type="text" name="textdata" value="foo">\n'
    '    <input type="submit" id="submit">\n'
    '</form>\n'
    '\n'
    '</body>\n'
    '</html>\n'
)