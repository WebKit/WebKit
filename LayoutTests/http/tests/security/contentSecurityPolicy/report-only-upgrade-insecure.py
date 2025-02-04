#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Content-Security-Policy-Report-Only: script-src \'self\'; upgrade-insecure-requests; report-uri resources/save-report.py\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<script>\n'
    '// This script block will trigger a violation report but shouldn\'t be blocked.\n'
    'alert(\'PASS\');\n'
    '</script>\n'
    '<script src="resources/go-to-echo-report.js"></script>\n'
)
