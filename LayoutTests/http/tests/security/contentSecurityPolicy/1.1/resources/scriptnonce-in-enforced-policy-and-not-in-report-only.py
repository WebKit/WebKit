#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy: script-src \'nonce-test\'\r\n'
    'Content-Security-Policy-Report-Only: script-src \'none\'\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<body>\n'
    '<p id="script-with-nonce-result">FAIL did not execute script with nonce.</p>\n'
    '<p id="script-without-nonce-result">PASS did not execute script without nonce.</p>\n'
    '<script nonce="test">\n'
    'document.getElementById("script-with-nonce-result").textContent = "PASS did execute script with nonce.";\n'
    '</script>\n'
    '<script>\n'
    'document.getElementById("script-without-nonce-result").textContent = "FAIL did execute script without nonce.";\n'
    '</script>\n'
    '</body>\n'
    '</html>\n'
)