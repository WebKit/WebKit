#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy: img-src \'none\'; report-uri resources/save-report.py\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<body>\n'
    '    <script src="resources/inject-image.js"></script>\n'
    '    <script src="resources/go-to-echo-report.js"></script>\n'
    '</body>\n'
    '</html>\n'
)