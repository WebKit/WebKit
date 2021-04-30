#!/usr/bin/env python3

import base64
import os
import sys

credentials = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')
username = credentials[0]
password = ':'.join(credentials[1:])

sys.stdout.write('Content-Type: text/html\r\n')

if not username:
    sys.stdout.write(
        'WWW-Authenticate: Basic realm="WebKit test - credentials-in-main-resource"\r\n'
        'status: 401\r\n\r\n'
    )
    sys.exit(0)

sys.stdout.write(
    '\r\nMain Resource Credentials: {}, {}<br/>'
    '<script>\n'
    '\n'
    'if (window.internals)\n'
    '    internals.settings.setStorageBlockingPolicy(\'BlockThirdParty\');\n'
    '\n'
    'var request = new XMLHttpRequest();\n'
    'request.onreadystatechange = function () {{\n'
    '    if (this.readyState == 4) {{\n'
    '        alert(this.responseText);\n'
    '        if (window.testRunner)\n'
    '            testRunner.notifyDone();\n'
    '    }}\n'
    '}};\n'
    'request.open(\'GET\', \'http://127.0.0.1:8000/security/resources/basic-auth.py?username=testuser&password=testpass\', true);\n'
    'request.send(null);\n'
    '</script>\n'.format(username, password)
)