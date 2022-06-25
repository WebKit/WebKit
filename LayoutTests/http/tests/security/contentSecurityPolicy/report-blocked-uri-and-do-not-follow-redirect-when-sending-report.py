#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy-Report-Only: img-src \'none\'; report-uri resources/save-report-and-redirect-to-save-report.py\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<body>\n'
    '<p>This test PASSED if the filename of the REQUEST_URI in the dumped report is save-report-and-redirect-to-save-report.py. Otherwise, it FAILED.</p>\n'
    '<img src="../resources/abe.png"> <!-- Trigger CSP violation -->\n'
    '<script>\n'
    'if (window.testRunner) {\n'
    '    testRunner.dumpAsText();\n'
    '    testRunner.waitUntilDone();\n'
    '}\n'
    '\n'
    'function navigateToReport()\n'
    '{\n'
    '    window.location = "/security/contentSecurityPolicy/resources/echo-report.py";\n'
    '}\n'
    '\n'
    '// We assume that if redirects were followed when saving the report that they will complete within one second.\n'
    '// FIXME: Is there are better way to test that a redirect did not occur?\n'
    'window.setTimeout(navigateToReport, 1000);\n'
    '</script>\n'
    '</body>\n'
    '</html>\n'
)