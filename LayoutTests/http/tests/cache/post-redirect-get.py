#!/usr/bin/env python3

import cgi
import os
import sys
import tempfile
from datetime import datetime, timedelta
from urllib.parse import parse_qs

# This test loads an uncacheable main resource and a cacheable image subresource.
# We then trigger a POST request, which redirects as a GET back to this page.
# On this GET request, the image should be loaded from the cache and no HTTP
# request should be sent. The image resource will return 404 if it receives
# a second request, which will cause us to FAIL.
# See https://bugs.webkit.org/show_bug.cgi?id=38690

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))

request_method = os.environ.get('REQUEST_METHOD', '')
expires = '{} +0000'.format((datetime.utcnow() - timedelta(seconds=1)).strftime('%a, %d %b %Y %H:%M:%S'))
finish = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('finish', [''])[0]

request = {}
if request_method == 'POST':
    form = cgi.FieldStorage()
    for key in sorted(form.keys()):
        request[key] = form.getvalue(key)

sys.stdout.write(
    'Cache-Control: no-cache, no-store, must-revalidate, max-age=0\r\n'
    'Pragma: no-cache\r\n'
    f'Expires: {expires}\r\n'
    'Content-Type: text/html\r\n'
)

if request.get('submit', '') == 'redirect':
    sys.stdout.write(
        'status: 302\r\n'
        'Location: post-redirect-get.py?finish=true\r\n\r\n'
    )
    sys.exit(0)

sys.stdout.write(
    '\r\n'
    '<html>\n'
    '<body>\n'
    '<div id="result"></div>\n'
    '<script>\n'
    'if (window.testRunner) {\n'
    '    testRunner.dumpAsText();\n'
    '    testRunner.waitUntilDone();\n'
    '}\n'
    '    \n'
    'function logAndFinish(message)\n'
    '{\n'
    '    document.getElementById("result").appendChild(document.createTextNode(message));\n'
    '    var xhr = new XMLHttpRequest;\n'
    '    xhr.open("GET", "../resources/reset-temp-file.py?filename=cache_post-redirect-get_state", false);\n'
    '    xhr.send(null);\n'
    '    if (window.testRunner)\n'
    '        testRunner.notifyDone();\n'
    '}\n'
    '</script>\n'
)

if finish == 'true':
    tmpFile = os.path.join(tempfile.gettempdir(), 'cache_post-redirect-get_state')
    if not os.path.isfile(tmpFile):
        sys.stdout.write(
            '<script>'
            'xhr = new XMLHttpRequest;'
            'xhr.open(\'GET\', \'../resources/touch-temp-file.py?filename=cache_post-redirect-get_state\', false);'
            'xhr.send(null);'
            '</script>'
        )
    sys.stdout.write('<img src=\'resources/post-image-to-verify.py?test=cache_post-redirect-get\' onload="logAndFinish(\'PASS\');" onerror="logAndFinish(\'FAIL\');"></img>')
else:
    sys.stdout.write(
        '<form action=\'post-redirect-get.py\' method=\'post\'>'
        '<input type=\'submit\' id=\'submit\' name=\'submit\' value=\'redirect\'>'
        '</form>'
        '<img src=\'resources/post-image-to-verify.py?test=cache_post-redirect-get\' onload="document.getElementById(\'submit\').click();" onerror="logAndFinish(\'FAIL on initial load\');"></img>'
    )

sys.stdout.write(
    '</body>\n'
    '</html>\n'
)