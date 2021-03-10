#!/usr/bin/env python3

import sys

sys.stdout.write(
    'X-XSS-Protection: 1; mode=block\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<head>\n'
    '<script src="http://127.0.0.1:8000/security/xssAuditor/resources/utilities.js"></script>\n'
    '<script>\n'
    'if (window.testRunner) {\n'
    '    testRunner.dumpAsText();\n'
    '    testRunner.dumpChildFramesAsText();\n'
    '    testRunner.waitUntilDone();\n'
    '    internals.settings.setXSSAuditorEnabled(true);\n'
    '}\n'
    '</script>\n'
    '</head>\n'
    '<body>\n'
    '<p>This tests that the header X-XSS-Protection is not inherited by the iframe below:</p>\n'
    '<iframe id="frame" name="frame" onload="checkIfFrameLocationMatchesSrcAndCallDone(\'frame\')" src="http://127.0.0.1:8000/security/xssAuditor/resources/echo-intertag.pl?test=/security/xssAuditor/full-block-iframe-no-inherit.html&q=<script>alert(/XSS/)</script><p>If you see this message and no JavaScript alert() then the test PASSED.</p>">\n'
    '</iframe>\n'
    '</body>\n'
    '</html>\n'
)