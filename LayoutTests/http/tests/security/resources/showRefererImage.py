#!/usr/bin/env python3

import os
import sys

referer = os.environ.get('HTTP_REFERER', '')

if 'file:' in referer.lower():
    sys.stdout.write('Location: http://127.0.0.1:8000/security/resources/red200x100.png\r\n')
else:
    sys.stdout.write('Location: http://127.0.0.1:8000/security/resources/green250x50.png\r\n')

sys.stdout.write('Content-Type: text/html\r\n\r\n')