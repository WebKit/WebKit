#!/usr/bin/env python3

import os
import sys
import tempfile
from urllib.parse import parse_qs

# This script may only be used by appcache/update-cache.html test, since it uses global data.

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_count, step_state

command = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('command', [''])[0]
tmp_file = os.path.join(tempfile.gettempdir(), 'appcache_update-cache_state')

sys.stdout.write(
    'Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n'
    'Cache-Control: no-cache, must-revalidate\r\n'
    'Pragma: no-cache\r\n'
    'Content-Type: text/cache-manifest\r\n\r\n'
)

if command == 'step':
    step_state(tmp_file)

sys.stdout.write(
    'CACHE MANIFEST\n'
    f'# version {get_count(tmp_file)}\n'
    'uncacheable-resource.py\n'
    'NETWORK:\n'
    'update-cache.py?command=\n'
)