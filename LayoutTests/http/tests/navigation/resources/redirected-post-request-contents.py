#!/usr/bin/env python3

import cgi
import os
import sys
from urllib.parse import parse_qs


def check_header(header):
    if header in os.environ.keys():
        sys.stdout.write('{} is present. Its value is: {}<br>'.format(header, os.environ.get(header)))
        return True
    return False


request_method = os.environ.get('REQUEST_METHOD', '')
request = {}
if request_method == 'POST':
    form = cgi.FieldStorage()
    for key in form.keys():
        request[key] = form.getvalue(key)

content = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('content', [None])[0]

sys.stdout.write('Content-Type: text/html\r\n\r\n')

if content == 'true':
    sys.stdout.write('headers CONTENT_TYPE and CONTENT_LENGTH should be present.<br>')
else:
    sys.stdout.write('headers CONTENT_TYPE and CONTENT_LENGTH should not be present.<br>')

content_type = check_header('CONTENT_TYPE')
content_length = check_header('CONTENT_LENGTH')

if not content_type and not content_length:
    sys.stdout.write('headers CONTENT_TYPE and CONTENT_LENGTH are not present.<br>')

sys.stdout.write('<br>')

if content == 'true':
    sys.stdout.write('POST data should be present.<br>')
else:
    sys.stdout.write('no POST data should be present.<br>')

if len(request) > 0:
    sys.stdout.write('POST data is present.<br>')
else:
    sys.stdout.write('no POST data is present.<br>')

sys.stdout.write('<script>if (window.testRunner) testRunner.notifyDone();</script>')
