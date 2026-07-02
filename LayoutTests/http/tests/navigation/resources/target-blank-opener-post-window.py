#!/usr/bin/env python3

import os
import sys

referer = os.environ.get('HTTP_REFERER', '')

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<p>Referrer: {referer}</p>\n'
    '<script>\n'
    'console.log("Referrer: " + ("{referer}" == "" ? "PASS" : "FAIL"));\n'
    '\n'
    'var result = "window.opener: " + (window.opener ? "FAIL" : "PASS");\n'
    'document.write(result);\n'
    'console.log(result);\n'
    'if (window.testRunner)\n'
    '    testRunner.notifyDone();\n'
    '</script>\n'.format(referer=referer)
)
