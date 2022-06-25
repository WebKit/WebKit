#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy-Report-Only: block-all-mixed-content; report-uri ../../resources/save-report.py?test=/security/contentSecurityPolicy/block-all-mixed-content/resources/frame-with-insecure-css-report-only.py\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<head>\n'
    '<style>\n'
    'body {\n'
    '    background-color: white;\n'
    '}\n'
    '</style>\n'
    '<link rel="stylesheet" href="http://127.0.0.1:8000/security/mixedContent/resources/style.css">\n'
    '</head>\n'
    '<body>\n'
    'This background color should be white.\n'
    '<script>\n'
    '    window.location.href = "/security/contentSecurityPolicy/resources/echo-report.py?test=/security/contentSecurityPolicy/block-all-mixed-content/resources/frame-with-insecure-css-report-only.py";\n'
    '</script>\n'
    '</body>\n'
    '</html>\n'
)