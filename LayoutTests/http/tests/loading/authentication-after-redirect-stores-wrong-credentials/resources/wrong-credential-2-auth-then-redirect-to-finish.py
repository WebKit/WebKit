#!/usr/bin/env python3

import base64
import os
import sys

username = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')[0]

sys.stdout.write(
    'Content-Type: text/html\r\n'
)

if not username:
    sys.stdout.write(
        'WWW-Authenticate: Basic realm="loading/authentication-after-redirect-stores-wrong-credentials"\r\n'
        'status: 401\r\n\r\n'
    )
else:
    sys.stdout.write(
        '\r\n'
        '<script>\n'
        '// This page was supposed to be loaded using a 127.0.0.1 URL.\n'
        '// That is important, and the final page has to be loaded using localhost.\n'
        '// Plus, the redirect to the final page in this test has to be a new page load to trigger the bug; It cannot be an HTTP redirect.\n'
        'window.setTimeout("window.location = \'http://localhost:8000/loading/authentication-after-redirect-stores-wrong-credentials/resources/wrong-credential-3-output-credentials-then-finish.py\';", 0);\n'
        '</script>\n'
    )