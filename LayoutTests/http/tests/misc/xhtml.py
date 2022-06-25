#!/usr/bin/env python3

import os
import sys

accept = os.environ.get('HTTP_ACCEPT', '')

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<html>\n'
    '<body>\n'
    '<script>\n'
    'if (window.testRunner)\n'
    '    testRunner.dumpAsText();\n'
    '\n'
    'var accept = "{}";\n'
    'document.write("Accept: " + accept + "<br><br>");\n'
    'if (accept.match(/application\\/xhtml\\+xml/)) {{\n'
    '    document.write("PASS: The browser asks for XHTML.");\n'
    '}} else {{\n'
    '    document.write("FAIL: The browser doesn\'t ask for XHTML");\n'
    '}}\n'
    '</script>\n'
    '\n'
    '</body>\n'
    '</html>\n'.format(accept)
)