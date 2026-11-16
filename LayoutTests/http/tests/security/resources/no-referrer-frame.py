#!/usr/bin/env python3

import os
import sys

referer = os.environ.get('HTTP_REFERER', '')

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<script>\n'
    'function log(message)\n'
    '{\n'
    '    parent.document.getElementById("log").innerHTML += message + "<br>";\n'
    '}\n'
    '\n'
    'if (document.referrer.toString() != "") {\n'
    '  log("JavaScript: FAIL");\n'
    '} else {\n'
    '  log("JavaScript: PASS");\n'
    '}\n'
)

if referer:
    sys.stdout.write('log(\'HTTP Referer: FAIL\')\n')
else:
    sys.stdout.write('log(\'HTTP Referer: PASS\')\n')

sys.stdout.write(
    'window.onload = function() {\n'
    '    var xhr = new XMLHttpRequest;\n'
    '    xhr.open("GET", "no-referrer.py", false);\n'
    '    xhr.send(null);\n'
    '    log("Sync XHR: " + (xhr.responseText.match(/HTTP.*FAIL/) ? "FAIL" : "PASS"));\n'
    '    xhr.open("GET", "no-referrer.py", true);\n'
    '    xhr.send(null);\n'
    '    xhr.onload = onXHRLoad;\n'
    '}\n'
    '\n'
    'function onXHRLoad(evt)\n'
    '{\n'
    '    log("ASync XHR: " + (evt.target.responseText.match(/HTTP.*FAIL/) ? "FAIL" : "PASS"));\n'
    '    log("DONE");\n'
    '    if (window.testRunner)\n'
    '        testRunner.notifyDone();\n'
    '}\n'
    '</script>\n'
    '<script src="no-referrer.py"></script>\n'
)