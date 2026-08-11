#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Refresh: 0, URL    =   "resources/redirect-step2.py"\r\n'
    'Content-Type: text/html\r\n\r\n\n'
    '<body>\n'
    '<script>\n'
    '   if (window.testRunner) {\n'
    '       testRunner.waitUntilDone();\n'
    '       testRunner.dumpAsText();\n'
    '   }\n'
    '</script>\n'
    '   \n'
    '<p>FAILURE - should redirect (1)<p>\n'
    '</body>\n'
)