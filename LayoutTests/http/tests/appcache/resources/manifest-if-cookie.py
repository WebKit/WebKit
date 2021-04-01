#!/usr/bin/env python3

import os
import sys

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_cookies

cookies = get_cookies()
name = cookies.get('name', None)

if name is not None:
    sys.stdout.write(
        'Content-Type: text/html; {}\r\n\r\n'
        'CACHE MANIFEST\n'
        'simple.txt\n'.format(name)
    )
    sys.exit(0)

sys.stdout.write(
    'status: 404\r\n'
    'Content-Type: text/html; {}\r\n\r\n'.format(len(cookies))
)