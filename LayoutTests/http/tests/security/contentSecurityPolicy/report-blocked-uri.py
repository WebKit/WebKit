#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy-Report-Only: img-src \'none\'; report-uri resources/save-report.py\r\n'
    'Content-Type: text/html\r\n\r\n'
    'The URI of this image should show up in the violation report.\n'
    '<img src="../resources/abe.png#the-fragment-should-not-be-in-report">\n'
    '<script src="resources/go-to-echo-report.js"></script>\n'
)