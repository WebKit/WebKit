#!/usr/bin/env python3

import sys

sys.stdout.write(
    "Content-Security-Policy: script-src 'nonce-dummy' 'strict-dynamic'; report-uri resources/save-report.py\r\n"
    'Content-Type: text/html\r\n\r\n'
    "<script>console.log('blocked');</script>\n"
    '<script nonce="dummy" src="resources/go-to-echo-report.js"></script>\n'
)
