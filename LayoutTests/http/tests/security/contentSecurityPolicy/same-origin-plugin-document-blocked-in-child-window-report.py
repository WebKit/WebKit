#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy: script-src \'self\' \'unsafe-inline\'; plugin-types application/x-webkit-dummy-plugin; report-uri /security/contentSecurityPolicy/resources/save-report.py?test=/security/contentSecurityPolicy/same-origin-plugin-document-blocked-in-child-window-report.py\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<head>\n'
    '<script src="resources/checkDidSameOriginChildWindowLoad.js"></script>\n'
    '<script>\n'
    'if (window.testRunner) {\n'
    '    testRunner.dumpAsText();\n'
    '    testRunner.setCanOpenWindows();\n'
    '    testRunner.waitUntilDone();\n'
    '}\n'
    '</script>\n'
    '</head>\n'
    '<body>\n'
    '<script>\n'
    'function navigateToCSPReport()\n'
    '{\n'
    '    window.location.href = "/security/contentSecurityPolicy/resources/echo-report.py?test=/security/contentSecurityPolicy/same-origin-plugin-document-blocked-in-child-window-report.py";\n'
    '}\n'
    '\n'
    'checkDidSameOriginChildWindowLoad(window.open("http://127.0.0.1:8000/plugins/resources/mock-plugin.pl"), navigateToCSPReport);\n'
    '</script>\n'
    '</body>\n'
    '</html>\n'
)
