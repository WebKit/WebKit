#!/usr/bin/env python3

import cgi
import os
import sys
from urllib.parse import parse_qs

method = os.environ.get('REQUEST_METHOD', '')

sys.stdout.write(
    'Content-Type: text/html; charset=UTF-8\r\n\r\n'
    '<html><!-- webkit-test-runner [ KeygenElementEnabled=true ] -->\n'
    '<head>\n'
    '<script>\n'
    '\n'
    'function runTest()\n'
    '{\n'
    '    var r = document.getElementById(\'result\');\n'
    '    var o = document.getElementById(\'output\').firstChild;\n'
    '    if (o.nodeValue == \'MIHFMHEwXDANBgkqhkiG9w0BAQEFAANLADBIAkEAnX0TILJrOMUue%2BPtwBRE6XfV%0AWtKQbsshxk5ZhcUwcwyvcnIq9b82QhJdoACdD34rqfCAIND46fXKQUnb0mvKzQID%0AAQABFhFNb3ppbGxhSXNNeUZyaWVuZDANBgkqhkiG9w0BAQQFAANBAAKv2Eex2n%2FS%0Ar%2F7iJNroWlSzSMtTiQTEB%2BADWHGj9u1xrUrOilq%2Fo2cuQxIfZcNZkYAkWP4DubqW%0Ai0%2F%2FrgBvmco%3D\')\n'
    '        r.innerHTML = "SUCCESS: keygen was parsed correctly";\n'
    '    else\n'
    '        r.innerHTML = "FAILURE: keygen was not parsed correctly. value=" +\n'
    '        o.nodeValue;\n'
    '        \n'
    '    if (window.testRunner)\n'
    '        testRunner.notifyDone();\n'
    '}\n'
    '\n'
    '</script>\n'
    '</head>\n'
    '<body onload="runTest()">\n'
    '<p>\n'
    'This is a regression test for keygen tag POST processing: https://bugs.webkit.org/show_bug.cgi?id=70617.\n'
    '</p>\n'
    '<div style=\'display: none;\' id=\'output\'>'
)

request = {}
if method == 'POST':
    form = cgi.FieldStorage()
    for key in form.keys():
        request[key] = form.getvalue(key)
else:
    query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
    for key in query.keys():
        request[key] = query[key]

if 'HTTP_COOKIE' in os.environ:
    header_cookies = os.environ['HTTP_COOKIE']
    header_cookies = header_cookies.split('; ')

    for cookie in header_cookies:
        cookie = cookie.split('=')
        request[cookie[0]] = cookie[1]

if 'spkac' in request.keys():
    sys.stdout.write('{}'.format(request['spkac']))
else:
    sys.stdout.write('spkac does not exist')

sys.stdout.write(
    '</div>\n'
    '<div id="result"></div>\n'
    '</body>\n'
    '</html>\n'
)