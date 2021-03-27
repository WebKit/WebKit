#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy-Report-Only: script-src \'nonce-dummy\' \'nonce-dump-as-text\'\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<head>\n'
    '<meta http-equiv="X-WebKit-CSP" content="script-src \'nonce-dump-as-text\'">\n'
    '<script nonce="dump-as-text">\n'
    'if (window.testRunner)\n'
    '    testRunner.dumpAsText();\n'
    '</script>\n'
    '</head>\n'
    '<body>\n'
    '<p id="result">FAIL did not execute script.</p>\n'
    '<script nonce="dummy">\n'
    'document.getElementById("result").textContent = "PASS did execute script.";\n'
    '</script>\n'
    '</body>\n'
    '</html>\n'
)