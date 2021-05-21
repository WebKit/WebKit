#!/usr/bin/env python3

import os
import sys

referer = os.environ.get('HTTP_REFERER', '')

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<p>Referrer: {referer}</p>\n'
    '<script>\n'
    'if (window.testRunner) {{\n'
    '    var didLoadAppBoundRequest = testRunner.didLoadAppBoundRequest();\n'
    '    var didLoadNonAppBoundRequest = testRunner.didLoadNonAppBoundRequest();\n'
    '    if (didLoadNonAppBoundRequest) {{\n'
    '        console.log("FAIL did load non app-bound request");\n'
    '        testRunner.notifyDone();\n'
    '    }}\n'
    '    if (!didLoadAppBoundRequest) {{\n'
    '         console.log("FAIL did not load app-bound request");\n'
    '        testRunner.notifyDone();\n'
    '     }}\n'
    '    console.log("PASS successfully loaded only app-bound requests");\n'
    '    testRunner.notifyDone();\n'
    '}}\n'
    '</script>\n'.format(referer=referer)
)
