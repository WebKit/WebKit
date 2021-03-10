#!/usr/bin/env python3

import os
import sys

referer = os.environ.get('HTTP_REFERER', '')

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<html>\n'
    '<head>\n'
    '<script>\n'
    'function ownerWindow()\n'
    '{{\n'
    '    var owner = window.parent;\n'
    '    if (owner === this)\n'
    '        owner = window.opener;\n'
    '    return owner;\n'
    '}}\n'
    '\n'
    'function log(message)\n'
    '{{\n'
    '    ownerWindow().postMessage(message, "*");\n'
    '}}\n'
    '\n'
    'function runTest()\n'
    '{{\n'
    '    var referrerHeader = "{}";\n'
    '    if (referrerHeader == "")\n'
    '        log("HTTP Referer header is empty");\n'
    '    else\n'
    '        log("HTTP Referer header is " + referrerHeader);\n'
    '\n'
    '    if (document.referrer == "")\n'
    '        log("Referrer is empty");\n'
    '    else\n'
    '        log("Referrer is " + document.referrer);\n'
    '\n'
    '    log("done");\n'
    '}}\n'
    '</script>\n'
    '</head>\n'
    '<body onload="runTest()">\n'
    '</body>\n'
    '</html>\n'.format(referer)
)