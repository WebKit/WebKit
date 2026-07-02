#!/usr/bin/env python3

import sys

sys.stdout.write(
    'status: 200\r\n'
    'Refresh: 0;URL=   javascript:window.top.location="about:blank"\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<body>\n'
    '<script>\n'
    'if (window.testRunner)\n'
    '    testRunner.dumpAsText();\n'
    '</script>\n'
    'PASS: This is the content that appears in place of a refresh.\n'
    '</body>\n'
)