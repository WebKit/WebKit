#!/usr/bin/env python3

import cgi
import os
import sys
import tempfile
from datetime import datetime, timedelta

# This test loads an uncacheable main resource and a cacheable image subresource.
# We request this page as a GET, then reload this page with a POST.
# On the post request, the image should be loaded from the cache and no HTTP
# request should be sent. The image resource will return 404 if it receives
# a second request, which will cause us to FAIL.
# See https://bugs.webkit.org/show_bug.cgi?id=38690

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))

request_method = os.environ.get('REQUEST_METHOD', '')
exp_time = datetime.utcnow() - timedelta(seconds=1)

sys.stdout.write(
    'Cache-Control: no-cache, no-store, must-revalidate, max-age=0\r\n'
    'Pragma: no-cache\r\n'
    'Expires: {} +0000\r\n'
    'Content-Type: text/html\r\n\r\n'.format(exp_time.strftime('%a, %d %b %Y %H:%M:%S'))
)

sys.stdout.write(
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
    '    xhr.open("GET", "../resources/reset-temp-file.py?filename=cache_post-with-cached-subresources_state", false);\n'
    '    xhr.send(null);\n'
    '    if (window.testRunner)\n'
    '        testRunner.notifyDone();\n'
    '}\n'
    '</script>\n'
)

request = {}
if request_method == 'POST':
    form = cgi.FieldStorage()
    for key in sorted(form.keys()):
        request[key] = form.getvalue(key)

if request.get('submit', '') == 'finish':
    tmpFilePath = os.path.join(tempfile.gettempdir(), 'cache_post-with-cached-subresources_state')
    if os.path.exists(tmpFilePath):
        sys.stdout.write(
            '<script>'
            'xhr = new XMLHttpRequest;'
            'xhr.open(\'GET\', \'../resources/touch-temp-file.py?filename=cache_post-with-cached-subresources_state\', false);'
            'xhr.send(null);'
            '</script>'
        )

    sys.stdout.write('<img src=\'resources/post-image-to-verify.py?test=cache_post-with-cached-subresources\' onload="logAndFinish(\'PASS\');" onerror="logAndFinish(\'FAIL\');"></img>')
else:
    sys.stdout.write(
        '<form action=\'post-with-cached-subresources.py\' method=\'post\'>'
        '<input type=\'submit\' id=\'submit\' name=\'submit\' value=\'finish\'>'
        '</form>'
        '<img src=\'resources/post-image-to-verify.py?test=cache_post-with-cached-subresources\' onload=\"document.getElementById(\'submit\').click();\" onerror=\"logAndFinish(\'FAIL on initial load\');\"></img>'
    )

sys.stdout.write(
    '</body>\n'
    '</html>\n'
)