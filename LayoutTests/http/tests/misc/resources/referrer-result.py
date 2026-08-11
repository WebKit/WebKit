#!/usr/bin/env python3

import os
import sys

referer = os.environ.get('HTTP_REFERER', '')

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<html>\n'
    '<body>\n'
    '<script>\n\n'
    'var referer = "{}";\n'
    'if (referer.match(/referrer.html/)) {{\n'
    '    document.write("PASS: Referer header exists and contains the string \'referrer.html\'.");\n'
    '}} else {{\n'
    '    document.write("FAIL: Referer header does not contain the string \'referrer.html\', value is \'" + referer + "\'");\n'
    '}}\n\n'
    'if (window.testRunner)\n'
    '    testRunner.notifyDone();\n'
    '</script>\n\n'
    '</body>\n'
    '</html>\n'.format(referer)
)