#!/usr/bin/env python3

import sys
import time

sys.stdout.write(
    'Content-Type: text/html; charset=utf-8\r\n\r\n'
    '<!-- {} -->\n'
    '<body>\n'
    '<script>\n'
    'if (window.testRunner)\n'
    '    testRunner.dumpAsText();\n'
    '\n'
    'function checkForPreload() {{\n'
    '    var result;\n'
    '    if (internals.isPreloaded("resources/preload-test.jpg"))\n'
    '        result = "PASS";\n'
    '    else\n'
    '        result = "FAIL";\n'
    '    document.getElementsByTagName("body")[0].appendChild(document.createTextNode(result));\n'
    '}}\n'
    '\n'
    'window.addEventListener("DOMContentLoaded", checkForPreload, false);\n'
    '\n'
    'function debug(x) {{}}\n'
    '</script>\n'
    '<script src="http://127.0.0.1:8000/resources/slow-script.pl?delay=1000"></script>\n'.format('A' * 2048)
)

sys.stdout.flush()
time.sleep(0.2)

sys.stdout.write(
    '<script>\n'
    'document.write("<plaintext>");\n'
    '</script>\n'
    'This test needs to be run in DRT. Preload scanner should see the image resource.\n'
    '<img src="resources/preload-test.jpg">\n'
)