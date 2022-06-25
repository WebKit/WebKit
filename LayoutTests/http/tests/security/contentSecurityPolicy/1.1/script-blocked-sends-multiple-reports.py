#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy-Report-Only: script-src http://example.com \'unsafe-inline\'; report-uri ../resources/save-report.py?test=script-blocked-sends-multiple-reports-report-only\r\n'
    'Content-Security-Policy: script-src http://127.0.0.1:8000 \'unsafe-inline\'; report-uri ../resources/save-report.py?test=script-blocked-sends-multiple-reports-enforced-1, script-src http://127.0.0.1:8000 https://127.0.0.1:8443 \'unsafe-inline\'; report-uri ../resources/save-report.py?test=script-blocked-sends-multiple-reports-enforced-2\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<head>\n'
    '<script>\n'
    'if (window.testRunner) {\n'
    '    testRunner.dumpAsText();\n'
    '    testRunner.dumpChildFramesAsText();\n'
    '}\n'
    '</script>\n'
    '</head>\n'
    '<body>\n'
    '<!-- Trigger CSP violation -->\n'
    '<script src="http://localhost:8000/security/contentSecurityPolicy/resources/alert-fail.js"></script>\n'
    '<!-- Reports -->\n'
    '<iframe name="report-only" src="../resources/echo-report.py?test=script-blocked-sends-multiple-reports-report-only"></iframe>\n'
    '<iframe name="enforced-1" src="../resources/echo-report.py?test=script-blocked-sends-multiple-reports-enforced-1"></iframe>\n'
    '<iframe name="enforced-2" src="../resources/echo-report.py?test=script-blocked-sends-multiple-reports-enforced-2"></iframe>\n'
    '</body>\n'
    '</html>\n'
)