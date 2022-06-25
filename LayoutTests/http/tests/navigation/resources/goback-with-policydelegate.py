#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<html>\n'
    '<head>\n'
    '<script>\n'
    '\n'
    'function loaded()\n'
    '{\n'
    '    if (window.testRunner)\n'
    '        testRunner.setCustomPolicyDelegate(true, true);\n'
    '\n'
    '    window.history.back();\n'
    '}\n'
    '\n'
    '</script>\n'
    '</head>\n'
    '<body onload="loaded();">\n'
    'This page turns on DRT\'s custom policy delegate then navigates back a page.\n'
    'If you\'re running the test in the browser, you should not get the form resubmission nag.\n'
    '</body>\n'
    '</html>\n'
)