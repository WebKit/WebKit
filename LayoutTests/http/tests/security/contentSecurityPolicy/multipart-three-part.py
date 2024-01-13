#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Type: multipart/x-mixed-replace; boundary=TEST\r\n'
    'Content-Security-Policy: sandbox\r\n\r\n'
    '--TEST\r\n'
    'Content-Type: text/html\r\n\r\n'
    '--TEST\r\n'
    'Content-Type: text/html\r\n'
    'Content-Security-Policy:\r\n\r\n'
    '<script>alert("FAIL")</script>\r\n'
    '--TEST--'
)
