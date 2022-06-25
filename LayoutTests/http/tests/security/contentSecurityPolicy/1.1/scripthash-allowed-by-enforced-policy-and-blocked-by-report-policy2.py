#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy-Report-Only: script-src \'sha256-33badf00d3badf00d3badf00d3badf00d3badf00d33=\' \'nonce-dump-as-text\'; report-uri ../resources/save-report.py?test=/security/contentSecurityPolicy/1.1/scripthash-allowed-by-enforced-policy-and-blocked-by-report-policy2.py\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<head>\n'
    '<meta http-equiv="Content-Security-Policy" content="script-src \'sha256-n7CDY/1Rg9w5XVqu2QuiqpjBw0MVHvwDmHpkLXsuu2g=\' \'nonce-dump-as-text\'">\n'
    '<script nonce="dump-as-text">\n'
    'if (window.testRunner) {\n'
    '    testRunner.dumpAsText();\n'
    '    testRunner.dumpChildFramesAsText();\n'
    '}\n'
    '</script>\n'
    '</head>\n'
    '<body>\n'
    '<p id="result">FAIL did not execute script.</p>\n'
    '<script>\n'
    'document.getElementById("result").textContent = "PASS did execute script.";\n'
    '</script>\n'
    '<iframe src="../resources/echo-report.py?test=/security/contentSecurityPolicy/1.1/scripthash-allowed-by-enforced-policy-and-blocked-by-report-policy2.py"></iframe>\n'
    '</body>\n'
    '</html>\n'
)