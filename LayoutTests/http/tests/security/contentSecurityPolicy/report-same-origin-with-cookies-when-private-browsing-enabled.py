#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy: img-src \'none\'; report-uri /security/contentSecurityPolicy/resources/save-report.py\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!-- webkit-test-runner [ useEphemeralSession=true ] -->\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<body>\n'
    '<script>\n'
    '    var xhr = new XMLHttpRequest();\n'
    '    xhr.open("GET", "/cookies/resources/setCookies.cgi", false);\n'
    '    xhr.setRequestHeader("SET-COOKIE", "hello=world;path=/");\n'
    '    xhr.send(null);\n'
    '</script>\n'
    '\n'
    '<!-- This image will generate a CSP violation report. -->\n'
    '<img src="/security/resources/abe.png">\n'
    '\n'
    '<script src="resources/go-to-echo-report.js"></script>\n'
    '</body>\n'
    '</html>\n'
)