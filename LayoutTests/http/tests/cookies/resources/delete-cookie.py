#!/usr/bin/env python3

import os
import sys
from datetime import datetime, timedelta
from urllib.parse import parse_qs

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_cookies

cookies = get_cookies()


def delete_cookie(name):
    expires = datetime.utcnow() - timedelta(seconds=86400)
    sys.stdout.write('Set-Cookie: {}=deleted; expires={} GMT; Max-Age=0; path=/\r\n'.format(name, expires.strftime('%a, %d-%b-%Y %H:%M:%S')))


query_function = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('queryfunction', [''])[0]

sys.stdout.write('Content-Type: text/html\r\n')

for cookie in cookies.keys():
    delete_cookie(cookie)

sys.stdout.write('\r\n<script>parent.postMessage(\'done\', \'*\');</script>\n')
