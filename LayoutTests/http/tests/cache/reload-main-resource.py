#!/usr/bin/env python3

import os
import sys
import tempfile

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))

tmpFile = os.path.join(tempfile.gettempdir(), 'cache-reload-main-resource.tmp')
if os.path.isfile(tmpFile):
    os.remove(tmpFile)

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<body>\n'
    '<div id="result"></div>\n'
    '<script>\n'
    'if (window.testRunner) {\n'
    '    testRunner.dumpAsText();\n'
    '    testRunner.waitUntilDone();\n'
    '}\n'
    '\n'
    'function reloadIframe() {\n'
    '    window.frames[0].location.reload();\n'
    '}\n'
    '\n'
    'function finish() {\n'
    '    document.getElementById("result").innerText = "PASS";\n'
    '    if (window.testRunner)\n'
    '        testRunner.notifyDone();\n'
    '}\n'
    '</script>\n'
    '<iframe src="resources/reload-main-resource-iframe.py" onload="reloadIframe();"></iframe>\n'
    '</body>\n'
)