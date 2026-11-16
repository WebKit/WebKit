#!/usr/bin/env python3

import os
import sys

referer = os.environ.get('HTTP_REFERER', '')

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<p>Referrer: {referer}</p>\n'
    '<script>\n'
    'if (window.testRunner) {{\n'
    '    var didLoadAppInitiatedRequest = testRunner.didLoadAppInitiatedRequest();\n'
    '    var didLoadNonAppInitiatedRequest = testRunner.didLoadNonAppInitiatedRequest();\n'
    '    if (didLoadNonAppInitiatedRequest) {{\n'
    '        console.log("FAIL did load non app initiated request");\n'
    '        testRunner.notifyDone();\n'
    '    }}\n'
    '    if (!didLoadAppInitiatedRequest) {{\n'
    '         console.log("FAIL did not load app initiated request");\n'
    '        testRunner.notifyDone();\n'
    '     }}\n'
    '    console.log("PASS successfully loaded only app initiated requests");\n'
    '    testRunner.notifyDone();\n'
    '}}\n'
    '</script>\n'.format(referer=referer)
)
