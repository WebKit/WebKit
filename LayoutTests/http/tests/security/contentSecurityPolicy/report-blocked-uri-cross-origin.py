#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy-Report-Only: img-src \'none\'; report-uri resources/save-report.py\r\n'
    'Content-Type: text/html\r\n\r\n'
    'The origin of this image should show up in the violation report.\n'
    '<img src="http://localhost:8080/security/resources/abe.png">\n'
    '<script src="resources/go-to-echo-report.js"></script>\n'
)