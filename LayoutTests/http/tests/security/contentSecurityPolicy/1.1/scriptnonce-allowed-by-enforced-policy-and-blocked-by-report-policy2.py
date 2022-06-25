#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy-Report-Only: script-src \'nonce-that-is-not-equal-to-dummy\' \'nonce-dump-as-text\'; report-uri ../resources/save-report.py?test=/security/contentSecurityPolicy/1.1/scriptnonce-allowed-by-enforced-policy-and-blocked-by-report-policy2.py\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<head>\n'
    '<meta http-equiv="Content-Security-Policy" content="script-src \'nonce-dummy\' \'nonce-dump-as-text\'">\n'
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
    '<!-- FIXME: Call testRunner.dumpChildFramesAsText() and load\n'
    '../resources/echo-report.py?test=/security/contentSecurityPolicy/1.1/scriptnonce-allowed-by-enforced-policy-and-blocked-by-report-policy2.py\n'
    'in an <iframe> once we fix reporting of nonce violations for report-only policies. See <https://bugs.webkit.org/show_bug.cgi?id=159830>. -->\n'
    '</body>\n'
    '</html>\n'
)