#!/usr/bin/env python3

import os
import sys

method = os.environ.get('REQUEST_METHOD', '')

sys.stdout.write('Content-Type: text/html\r\n')

if method == 'POST':
    sys.stdout.write(
        'Location: post-redirect-get-reload.py\r\n'
        'status: 303\r\n\r\n'
    )
    sys.exit(0)

sys.stdout.write(
    '\r\n'
    '<body>\n'
    '1. Submit a form<br>\n'
    '1a. The form redirects to a get.<br>\n'
    '2. Reload<br><br>\n'
    'The reload should not trigger a form resubmission warning.\n'
    '\n'
    '<form name="form" action="post-redirect-get-reload.py" method="post"><input type="submit"></input></form>\n'
    '<script>\n'
    '\n'
    'if (window.testRunner) {\n'
    '    testRunner.dumpAsText();\n'
    '    testRunner.waitUntilDone();\n'
    '\n'
    '    if (window.sessionStorage["prgl-state"] == null) {\n'
    '        window.sessionStorage["prgl-state"] = \'submitted\';\n'
    '        document.form.submit();\n'
    '    } else {\n'
    '        window.sessionStorage.clear();\n'
    '        testRunner.waitForPolicyDelegate();\n'
    '        window.internals.forceReload(false);\n'
    '    }\n'
    '}\n'
    '</script>\n'
)