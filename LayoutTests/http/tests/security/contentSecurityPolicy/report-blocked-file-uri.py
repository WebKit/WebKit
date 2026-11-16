#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy: img-src \'none\'; report-uri resources/save-report.py\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<body>\n'
    '    <script>\n'
    '        testRunner.addOriginAccessAllowListEntry(\'http://127.0.0.1:8000\', \'file\', \'\', true);\n'
    '        var localImageLocation = testRunner.pathToLocalResource(\'file:///tmp/LayoutTests/http/tests/security/resources/compass.jpg\');\n'
    '\n'
    '        var localImageElement = document.createElement(\'img\');\n'
    '        localImageElement.src = localImageLocation;\n'
    '        document.body.appendChild(localImageElement);\n'
    '    </script>\n'
    '    <script src="resources/go-to-echo-report.js"></script>\n'
    '</body>\n'
    '</html>\n'
)