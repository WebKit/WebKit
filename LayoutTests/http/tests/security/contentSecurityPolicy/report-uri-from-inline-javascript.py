#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy: img-src \'none\'; report-uri resources/save-report.py\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<body>\n'
    '    <script>\n'
    '        // This script block will trigger a violation report.\n'
    '        var i = document.createElement(\'img\');\n'
    '        i.src = \'/security/resources/abe.png\';\n'
    '        document.body.appendChild(i);\n'
    '    </script>\n'
    '    <script src="resources/go-to-echo-report.js"></script>\n'
    '</body>\n'
    '</html>\n'
)