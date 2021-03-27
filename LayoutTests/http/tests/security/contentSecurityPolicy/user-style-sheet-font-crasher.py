#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy: font-src http://webkit.org; report-uri http://webkit.org/report;\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<html>\n'
    '<head>\n'
    '<script>\n'
    'if (window.testRunner) {\n'
    '    testRunner.dumpAsText();\n'
    '    testRunner.waitUntilDone();\n'
    '    testRunner.addUserStyleSheet("@font-face { font-family: ExampleFont; src: url(example_font.woff); }", true);\n'
    '}\n'
    '</script>\n'
    '</head>\n'
    '<body>\n'
    'The iframe below triggers a violation report creating the initial empty document. It should not crash the web process.<br>\n'
    '<iframe src="http://127.0.0.1:8000/resources/notify-done.html"></iframe>\n'
    '</body>\n'
    '</html>\n'
)