#!/usr/bin/env python3

import base64
import os
import sys

username = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')[0]

sys.stdout.write('Content-Type: text/html\r\n')

if not username:
    sys.stdout.write(
        'WWW-Authenticate: Basic realm="WebKit test - credentials-in-referer"\r\n'
        'status: 401\r\n\r\n'
        'Authentication canceled'
    )
    sys.exit(0)

sys.stdout.write(
    '\r\n<script>\n'
    'if (window.testRunner) {\n'
    '    testRunner.waitUntilDone();\n'
    '    testRunner.dumpAsText();\n'
    '}\n'
    '\n'
    'function log(message)\n'
    '{\n'
    '    parent.document.getElementById("log").innerHTML += message + "<br>";\n'
    '}\n'
    '\n'
    'window.onload = function() {\n'
    '    var xhr = new XMLHttpRequest;\n'
    '    xhr.open("GET", "credentials-in-referer.py", false);\n'
    '    xhr.send(null);\n'
    '    log("Sync XHR: " + (xhr.responseText.match(/FAIL/) ? "FAIL" : "PASS"));\n'
    '\n'
    '    xhr.open("GET", "credentials-in-referer.py", true);\n'
    '    xhr.send(null);\n'
    '    xhr.onload = onXHRLoad;\n'
    '}\n'
    '\n'
    'function onXHRLoad(evt)\n'
    '{\n'
    '    log("Async XHR: " + (evt.target.responseText.match(/FAIL/) ? "FAIL" : "PASS"));\n'
    '    log("DONE");\n'
    '    if (window.testRunner)\n'
    '        testRunner.notifyDone();\n'
    '}\n'
    '</script>\n'
    '<script src="credentials-in-referer.py"></script>\n'
)