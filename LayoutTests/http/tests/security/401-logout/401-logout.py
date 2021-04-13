#!/usr/bin/env python3

import base64
import os
import sys

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_request

username = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')[0]
request = get_request()

sys.stdout.write('Content-Type: text/html\r\n')

if request.get('uid', None) is None:
    # Step 1 - navigate to a page that will make us remember credentials.
    sys.stdout.write(
        '\r\n<script>\n'
        'if (!window.testRunner) {\n'
        '    document.write(\'This test only works as an automated one\');\n'
        '    throw 0;\n'
        '}\n'
        'testRunner.waitUntilDone();\n'
        'testRunner.dumpAsText();\n'
        'testRunner.setHandlesAuthenticationChallenges(true)\n'
        'testRunner.setAuthenticationUsername(\'username\')\n'
        'testRunner.setAuthenticationPassword(\'password\')\n'
        'location = \'http://127.0.0.1:8000/security/401-logout/401-logout.py?uid=username\';\n'
        '</script>\n'
    )

elif not username or request.get('uid', '') != username:
    if request.get('laststep', None) is not None:
        # Step 4 - Credentials are no longer being sent
        sys.stdout.write(
            '\r\nPASS<script>\n'
            'if (window.testRunner) {\n'
            '    testRunner.notifyDone();\n'
            '}\n'
            '</script>\n'
        )
    else:
        # Ask for credentials is there are none
        sys.stdout.write(
            'WWW-Authenticate: Basic realm="401-logout"\r\n'
            'status: 401\r\n\r\n'
        )
else:
    if request.get('logout', None) is None:
        # Step 2 - navigate to a page that will make us forget the credentials
        sys.stdout.write(
            '\r\n<script>\n'
            'testRunner.setHandlesAuthenticationChallenges(false)\n'
            'location = \'http://127.0.0.1:8000/security/401-logout/401-logout.py?uid=username&logout=1\';\n'
            '</script>\n'
        )
    else:
        # Step 3 - logout
        sys.stdout.write(
            'WWW-Authenticate: Basic realm="401-logout"\r\n'
            'status: 401\r\n\r\n'
            '<script>\n'
            'location = \'http://127.0.0.1:8000/security/401-logout/401-logout.py?uid=username&laststep=1\';\n'
            '</script>\n'
        )
