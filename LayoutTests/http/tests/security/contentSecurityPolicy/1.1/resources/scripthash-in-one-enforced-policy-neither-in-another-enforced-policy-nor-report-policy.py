#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy-Report-Only: object-src \'none\'\r\n'
    'Content-Security-Policy: script-src \'sha256-+RQEi26Zq9zK8V7JRS2gLaVGlBEZ+fL8HA0f7zo5jUk=\', img-src \'none\'\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<body>\n'
    '<p id="result">FAIL did not execute script.</p>\n'
    '<script>document.getElementById("result").textContent = "PASS did execute script.";</script>\n'
    '</body>\n'
    '</html>\n'
)