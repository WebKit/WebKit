#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy-Report-Only: script-src \'nonce-dummy\' \'nonce-dump-as-text\'\r\n'
    'Content-Security-Policy: script-src \'nonce-that-is-not-equal-to-dummy\' \'nonce-dump-as-text\'; report-uri ../resources/save-report.py?test=/security/contentSecurityPolicy/1.1/scriptnonce-blocked-by-enforced-policy-and-allowed-by-report-policy.py\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<head>\n'
    '<script nonce="dump-as-text">\n'
    'if (window.testRunner) {\n'
    '    testRunner.dumpAsText();\n'
    '    testRunner.dumpChildFramesAsText();\n'
    '}\n'
    '</script>\n'
    '</head>\n'
    '<body>\n'
    '<p id="result">PASS did not execute script.</p>\n'
    '<script nonce="dummy">\n'
    'document.getElementById("result").textContent = "FAIL did execute script.";\n'
    '</script>\n'
    '<iframe src="../resources/echo-report.py?test=/security/contentSecurityPolicy/1.1/scriptnonce-blocked-by-enforced-policy-and-allowed-by-report-policy.py"></iframe>\n'
    '</body>\n'
    '</html>\n'
)