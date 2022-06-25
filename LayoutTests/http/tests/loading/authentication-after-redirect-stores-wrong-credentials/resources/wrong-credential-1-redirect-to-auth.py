#!/usr/bin/env python3

import sys

# This page was supposed to be loaded using a localhost URL.
# That is important, and the next page has to be loaded using 127.0.0.1.

sys.stdout.write(
    'Location: http://127.0.0.1:8000/loading/authentication-after-redirect-stores-wrong-credentials/resources/wrong-credential-2-auth-then-redirect-to-finish.py\r\n'
    'status: 302\r\n'
    'Content-Type: text/html\r\n\r\n'
)