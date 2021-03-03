#!/usr/bin/env python3

import os
import sys

method = os.environ.get('REQUEST_METHOD', '')

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<html>\n'
    '<head>\n'
    '<script>\n'
    'function test()\n'
    '{{\n'
    '    if (sessionStorage["state"] != 1) {{\n'
    '        sessionStorage["state"] = 1;\n'
    '        history.replaceState("new state", "new title");\n'
    '        history.go(-1);\n'
    '    }} else {{\n'
    '        sessionStorage.removeItem("state");\n'
    '        if (window.testRunner)\n'
    '            testRunner.notifyDone();\n'
    '    }}\n'
    '}}\n'
    '</script>\n'
    '</head>\n'
    '<body onload="test()">\n'
    '<p>\n'
    'If the current entry in the session history represents a non-GET request\n'
    '(e.g. it was the result of a POST submission) then update it to instead\n'
    'represent a GET request (or equivalent).\n'
    '</p>\n'
    '<p>\n'
    'This test checks that this works when navigation back and forward again.\n'
    '</p>\n'
    '<p>\n'
    'This test passes if this page is eventually loaded with a GET request.\n'
    '</p>\n'
    '<p>\n'
    'This page was loaded with a {} request.\n'
    '</p>\n'
    '</body>\n'.format(method)
)