#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy-Report-Only: script-src \'nonce-that-is-not-equal-to-dummy\' \'nonce-dump-as-text\'; report-uri ../resources/save-report.py?test=/security/contentSecurityPolicy/1.1/scriptnonce-blocked-by-legacy-enforced-policy-and-blocked-by-report-policy.py\r\n'
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
    '<!-- Call testRunner.dumpChildFramesAsText() and load\n'
    '<iframe src="../resources/echo-report.py?test=/security/contentSecurityPolicy/1.1/scriptnonce-blocked-by-legacy-enforced-policy-and-blocked-by-report-policy.py"></iframe>\n'
    'once we fix reporting of nonce violations for report-only policies. See <https://bugs.webkit.org/show_bug.cgi?id=159830>. -->\n'
    '</body>\n'
    '</html>\n'
)