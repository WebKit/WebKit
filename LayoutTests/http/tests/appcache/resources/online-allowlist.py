#!/usr/bin/env python3

import os
import sys
import tempfile

# This script may only be used by appcache/online-allowlist.html test, since it uses global data.

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import step_state

tmp_file = os.path.join(tempfile.gettempdir(), 'appcache_online-allowlist_state')

sys.stdout.write(
    'Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n'
    'Cache-Control: no-cache, must-revalidate\r\n'
    'Pragma: no-cache\r\n'
    'Content-Type: text/plain\r\n\r\n'
    f'{step_state(tmp_file)}'
)