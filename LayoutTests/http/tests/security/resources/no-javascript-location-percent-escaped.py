#!/usr/bin/env python3

import sys

sys.stdout.write(
    'status: 200\r\n'
    'Location: %6A%61%76%61%73%63%72%69%70%74:window.top.location="about:blank"\r\n'
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