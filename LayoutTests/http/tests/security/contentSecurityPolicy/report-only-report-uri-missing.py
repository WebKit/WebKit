#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy-Report-Only: script-src \'unsafe-inline\';\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<head>\n'
    '<script>\n'
    'if (window.testRunner)\n'
    '    testRunner.dumpAsText();\n'
    '</script>\n'
    '</head>\n'
    '<body>\n'
    '<p>This test passes if a console message is present, warning about the missing \'report-uri\' directive.</p>\n'
    '</body>\n'
    '</html>\n'
)