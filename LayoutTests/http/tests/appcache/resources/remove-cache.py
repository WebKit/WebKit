#!/usr/bin/env python3

import os
import sys
import tempfile
from urllib.parse import parse_qs

# This script may only be used by appcache/remove-cache.html test, since it uses global data.

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_state, set_state

command = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('command', [''])[0]
tmp_file = os.path.join(tempfile.gettempdir(), 'appcache_remove-cache_state')
state = get_state(tmp_file)

sys.stdout.write(
    'Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n'
    'Cache-Control: no-cache, must-revalidate\r\n'
    'Pragma: no-cache\r\n'
)

if command == 'reset':
    if os.path.isfile(tmp_file):
        os.remove(tmp_file)
    sys.stdout.write('Content-Type: text/html\r\n\r\n')
elif command == 'delete':
    set_state(tmp_file, 'Deleted')
    sys.stdout.write('Content-Type: text/html\r\n\r\n')
elif state == 'Uninitialized':
    sys.stdout.write(
        'Content-Type: text/cache-manifest\r\n\r\n'
        'CACHE MANIFEST\n'
        'NETWORK:\n'
        'remove-cache.py?command=\n'
    )
elif state == 'Deleted':
    sys.stdout.write(
        'status: 404\r\n'
        'Content-Type: text/html\r\n\r\n'
    )
else:
    sys.stodut.write('Content-Type: text/html\r\n\r\n')
