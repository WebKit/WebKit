#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy-Report-Only: block-all-mixed-content\r\n'
    'Content-Security-Policy: block-all-mixed-content\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<head>\n'
    '<script src="dump-securitypolicyviolation-and-notify-done.js"></script>\n'
    '</head>\n'
    '<body>\n'
    '<script>\n'
    '// Use document.write() to bypass the HTMLPreloadScanner and address flakiness.\n'
    'document.write(\'<img src="http://127.0.0.1:8000/security/resources/compass.jpg">\');\n'
    '</script>\n'
    '</body>\n'
    '</html>\n'
)