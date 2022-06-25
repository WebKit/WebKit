#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy: script-src \'self\'; report-uri //127.0.0.1:8080/security/contentSecurityPolicy/resources/save-report.py\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<meta name="referrer" content="unsafe-url">\n'
    '<script>\n'
    '// This script block will trigger a violation report.\n'
    'alert(\'FAIL\');\n'
    '</script>\n'
    '<script src="resources/go-to-echo-report.js"></script>\n'
)
