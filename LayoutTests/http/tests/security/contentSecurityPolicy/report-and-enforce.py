#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy-Report-Only: script-src \'self\'; report-uri resources/save-report.py\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<head>\n'
    '    <meta http-equiv="Content-Security-Policy" content="img-src \'none\'">\n'
    '</head>\n'
    '<body>\n'
    '    This image should be blocked, but should not show up in the violation report.\n'
    '    <img src="../resources/abe.png">\n'
    '    <script>\n'
    '        // This script block will trigger a violation report but shouldn\'t be blocked.\n'
    '        alert(\'PASS\');\n'
    '    </script>\n'
    '    <script src="resources/go-to-echo-report.js"></script>\n'
    '</body>\n'
    '</html>\n'
)