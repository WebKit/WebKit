#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy: script-src \'sha256-c8f8z1SC90Yj05k41+FT0HF/rrGJP94TPLhRvGGE8eM=\'\r\n'
    'Content-Security-Policy-Report-Only: script-src \'none\'\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<body>\n'
    '<p id="script-with-hash-result">FAIL did not execute script with hash.</p>\n'
    '<p id="script-without-hash-result">PASS did not execute script without hash.</p>\n'
    '<script>document.getElementById("script-with-hash-result").textContent = "PASS did execute script with hash.";</script> <!-- sha256-c8f8z1SC90Yj05k41+FT0HF/rrGJP94TPLhRvGGE8eM= -->\n'
    '<script>document.getElementById("script-without-hash-result").textContent = "FAIL did execute script without hash.";</script>\n'
    '</body>\n'
    '</html>\n'
)