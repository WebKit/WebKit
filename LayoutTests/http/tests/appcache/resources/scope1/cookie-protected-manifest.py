#!/usr/bin/env python3

import os
import sys

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file)))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_cookies
cookies = get_cookies()

scope = cookies.get('scope', None)
if scope is not None and scope == 'manifest':
    sys.stdout.write(
        'Content-Type: text/html; {}\r\n\r\n'
        'CACHE MANIFEST\n'
        '/appcache/resources/simple.txt\n'
        '/appcache/resources/scope2/cookie-protected-script.py\n'
        '/appcache/resources/cookie-protected-script.py\n'.format(scope)
    )
    sys.exit(0)

sys.stdout.write(
    'status: 404\r\n'
    'Content-Type: text/html; {}\r\n\r\n'.format(cookies.get('scope', ''))
)
