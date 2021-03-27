#!/usr/bin/env python3

import sys

sys.stdout.write(
    'status: 200\r\n'
    'Location: javascript:window.top.location="about:blank"\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<body>\n'
    '<script>\n'
    'if (window.testRunner)\n'
    '    testRunner.dumpAsText();\n'
    '</script>\n'
    'FAIL: This content should not appear after a failed Location redirect to a javascript: URL.\n'
    '</body>\n'
)