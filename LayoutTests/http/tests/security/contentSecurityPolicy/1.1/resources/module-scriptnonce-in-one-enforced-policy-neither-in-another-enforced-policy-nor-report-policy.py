#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy-Report-Only: object-src \'none\'\r\n'
    'Content-Security-Policy: script-src \'nonce-test\', img-src \'none\'\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<body>\n'
    '<p id="result">FAIL did not execute script.</p>\n'
    '<script type="module" nonce="test">\n'
    'document.getElementById("result").textContent = "PASS did execute script.";\n'
    'if (window.testRunner)\n'
    '    testRunner.notifyDone();\n'
    '</script>\n'
    '</body>\n'
    '</html>\n'
)