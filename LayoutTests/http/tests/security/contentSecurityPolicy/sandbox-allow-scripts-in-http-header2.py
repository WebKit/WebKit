#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy: sandbox allow-scripts allow-modals\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<body>\n'
    '<script>\n'
    'if (window.testRunner)\n'
    '    testRunner.dumpAsText();\n'
    '</script>\n'
    'This test passes if it does alert pass.\n'
    '<script>\n'
    'alert(\'PASS\');\n'
    '</script>\n'
    '</body>\n'
    '</html>\n'
)